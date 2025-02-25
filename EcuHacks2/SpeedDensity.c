#include "EcuHacks.h"

// The actual tables are defined in asm, simply because it's easier to tell
// the linker to put all of the data into the segment of your choice.
extern ThreeDimensionalTable VolumetricEfficiencyTable;
extern ThreeDimensionalTable TemperatureCompensationTable;
extern ThreeDimensionalTable AtmosphericCompensationTable;

// This is the function that will use speed-density math to compute the MAF
// value that will be returned to the rest of the ECU.  The function needs to
// be declared with the section attribute before it is actually defined, in 
// order to tell the compiler and linker which segment (address range) to put
// it in.
float ComputeMassAirFlow(int MafScalingTable, float MafVoltage)  __attribute__ ((section ("RomHole_SpeedDensityCode")));

// And finally we come to the body of the ComputeMassAirFlow function.
// Since this will be called from the location that formerly called Pull2D to
// get the MassAirFlow value from the MAF Scaling table, the first thing we do
// is call Pull2D with the MAF Scaling table and MAF voltage parameters that
// are already present in r4 and fr5.  And the last thing we do is return 
// whatever MAF value we want the ECU to use, in fr0 (either from the sensor,
// or from the SD math, depending on the Mode switch).
float ComputeMassAirFlow(int MafScalingTable, float MafVoltage)
{	
	// You might notice that the compiler generates code that copies the 
	// arguments (table and voltage) from registers onto the stack.  The stack 
	// variables are never used, but that's no big deal. It wastes a few clock
	// cycles but it makes it easy to write unit tests for this code.  We could
	// specify the arguments explicitly when calling Pull2d below, but that would
	// waste even more clock cycles.
	pRamVariables->MafFromSensor = Pull2dHooked();
			
	// We use a RAM variable here to determine which mode to use.  If the value 
	// is zero, we initialize it to whatever was specified in the MAF Mode table.
	// We could in theory toggle between modes without flashing, if someone wants
	// to write a tool for that.  However the real reason for this was just to 
	// make unit testing easier.
	if (pRamVariables->MafMode == MafModeUndefined)
	{
		pRamVariables->MafMode = DefaultMafMode;
	}
	
	// Note that RPM goes into fr5, and MAP into fr4.
	// The "*" means "load or store the value at this memory location," depending
	// on context.  This is called "dereferencing" a pointer.
	pRamVariables->VolumetricEfficiency = Pull3d(&VolumetricEfficiencyTable, *pManifoldAbsolutePressure, *pRPM);
	
	// Convert Celsius to Kelvin.  This will only be stored on the stack.
	// I wrapped the temperature variable and the dereferencing operator in 
	// parentheses, in hopes of making this easier to read.
	float intakeAirTempInKelvin = (*pIntakeAirTemperature) + CelsiusToKelvin;
	
	// Look up atmospheric pressure compensation	
	pRamVariables->AtmosphericCompensation = Pull3d(&AtmosphericCompensationTable, *pManifoldAbsolutePressure, *pAtmosphericPressure);
		
	// Do the speed-density math, with more parentheses for readability.  
	// When * appears immediately before a variable, it's dereferencing.  
	// When * appears between two variables, it's multiplying.  
	// Hopefully the parentheses make it clearer...
	pRamVariables->MafFromSpeedDensity = 
		Displacement * 
		(*pRPM) * 
		(*pManifoldAbsolutePressure) * 
		pRamVariables->VolumetricEfficiency * 
		pRamVariables->AtmosphericCompensation *
		SpeedDensityConstant / intakeAirTempInKelvin;
						
	// Return the value specified by the MAF mode variable.
	if (pRamVariables->MafMode == MafModeSpeedDensity)
	{
		return pRamVariables->MafFromSpeedDensity;
	}
	else
	{
		return pRamVariables->MafFromSensor;
	}
}

// This is only here to get the debugger to display the addresses that need
// to go into the table definition XML. 
void GetSpeedDensityTableInfo()  __attribute__ ((section ("Misc")));
void GetSpeedDensityTableInfo()
{
	void* address = 0;
	
	address = VolumetricEfficiencyTable.columnHeaderArray;
	address = VolumetricEfficiencyTable.rowHeaderArray;
	address = VolumetricEfficiencyTable.tableCells;
	
	address = AtmosphericCompensationTable.columnHeaderArray;
	address = AtmosphericCompensationTable.rowHeaderArray;
	address = AtmosphericCompensationTable.tableCells;
		
	address = &Displacement;
	address = &DefaultMafMode;
}