/* Description:		This program reads pressure transducer data from a ni USB-6002 and writes to a file.
*					See https://wiki.freepascal.org/NI-DAQmx_and_NI-DAQmx_Base_examples for examples using 
*					the NIDAQmx api.
*  Author:			Austin Andrews
*  Email:			andre889@umn.edu
*  Date:			March 5, 2021
*  Language:		C++ 14 using the MSVC v142 compiler
*/

#define _CRT_SECURE_NO_WARNINGS //Work around to allow std::locattime to be used. 
#include <iostream>
#include "NIDAQmx.h" //This header is from NI, will also need the compiled library. Found 
                     //C:\Program Files (x86)\National Instruments\NI-DAQ\DAQmx ANSI C Dev
#include <chrono>
#include <numeric>
#include <vector>
#include <ctime>   
#include <sstream> 
#include <iomanip> 
#include <string>  
#include <fstream>

//Macro for error handling 
#define DAQmxErrChk(functionCall) if( DAQmxFailed(error=(functionCall)) ) goto Error; else

//Function from https://stackoverflow.com/questions/17223096/outputting-date-and-time-in-c-using-stdchrono.
//Returns string with current data and time.
std::string return_current_time_and_date()
{
	auto now = std::chrono::system_clock::now();
	auto in_time_t = std::chrono::system_clock::to_time_t(now);

	std::stringstream ss;
	ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X");
	return ss.str();

}

int main()
{

	typedef std::vector<double> Yahye; //Teaching Yahye typedef

	int32       error = 0;
	TaskHandle  taskHandle = 0;
	char        errBuff[2048] = { '\0' };
	int32		reads = 0;

	double		frequency = 100.0;		//Frequency to sample in Hz
	size_t		bufferSize = 100;	    //Size of data buffer
	size_t		numberOfBuffers = 60;	//Number of "buffers" to average for file output. 
										//Example if frequency is 100 hz and the buffer is set to 100, 
	//then when reading from the DAQ you will recieve 1 second (buffersize/freq) worth of data.
	//If you want to average over 1 minute, then you will need to store the average of 60 1-sec buffers. 
	//Note: it may be possible to increase the buffer size to the desired time, in this case from 100 to 6000. 
	//This buffer will be stored on the DAQs memory and I am unsure of the upper limit.

	Yahye dataBuffer(bufferSize);					//Will transfer data from the DAQ's internal buffer to this array.
	Yahye bufferAverages(numberOfBuffers);			//Stores "numberOfBuffers" averages of internal buffers.
	size_t numberOfSamples = 5000;					//Number of times to repeat reading in numberOfBuffers. 
													//For example, in this case each sample is a min average, 
													//so this number represents number of minuets the program will run.
	
	std::ofstream myfile;

	//Create "Task" for the DAQ. This task can only handle reading in analog voltages. 
	//If you want to write voltages a new Task must be created.
	DAQmxErrChk(DAQmxCreateTask("", &taskHandle));

	//Set up the channel Dev1/ai0 to be DAQmx_Val_Diff (differential) and from 1 - 5 volts. Use standard units of Volts when reading in data.
	DAQmxErrChk(DAQmxCreateAIVoltageChan(taskHandle, "Dev1/ai0", "", DAQmx_Val_Diff, 1, 5, DAQmx_Val_Volts, NULL));

	//Sets frequency and tells DAQ to continously sample and put data into internal buffer of length bufferSize.
	DAQmxErrChk(DAQmxCfgSampClkTiming(taskHandle, "OnboardClock", frequency, DAQmx_Val_Rising, DAQmx_Val_ContSamps, bufferSize)); 
	
	//Start sampleing
	DAQmxErrChk(DAQmxStartTask(taskHandle));

	std::cout << "Start " << std::endl;
	
	myfile.open("dataPressureTransducer.csv");
	//Header
	myfile << "Date and Time , Pressure [PSI]\n";

	//Loop over every sample
	for (size_t samplei = 0; samplei != numberOfSamples; ++samplei)
	{
		//Loop over number of buffers
		for (size_t bufferi = 0; bufferi != numberOfBuffers; ++bufferi)
		{
			
			DAQmxErrChk(
				DAQmxReadAnalogF64(taskHandle, 
								  bufferSize,				//Number of values to read from each channel 
								  10.0,						//Time out in seconds
								  DAQmx_Val_GroupByChannel, //Data format (only usefull if using multiple channels
								  dataBuffer.data(),		//Read data from internal buffer to this array
								  bufferSize,				//Size of data buffer. Will be number of channels x number of values from each channel.
															//Here I am using one channel.
								  &reads,					//Returns how much data was read, maybe should be used for error handling?
								  NULL)						//Not used.
			);

			double meani = (double)std::accumulate(dataBuffer.begin(), dataBuffer.end(), 0.0) / (double)dataBuffer.size();

			//Convert from voltage to PSI. 
			meani = (15.0 / 4.0) * (meani - 1.0);

			std::cout << "mean value [PSI] so far: " << meani << " with these many samples " << reads << std::endl;
			bufferAverages[bufferi] = meani;
		}

		{
			double meanOfMeans = (double)std::accumulate(bufferAverages.begin(), bufferAverages.end(), 0.0) / (double)bufferAverages.size();
			
			std::cout << "*************************\nMean value [PSI] Last Min: \n*************************\n" << meanOfMeans << std::endl;

			//Write to file
			myfile << return_current_time_and_date() << "," << meanOfMeans << std::endl;
		}
	}
	
	myfile.close();

	//DAQ error handling, will print to console.
Error:
	if (DAQmxFailed(error))
		DAQmxGetExtendedErrorInfo(errBuff, 2048);
	if (taskHandle != 0) {

		DAQmxStopTask(taskHandle);
		DAQmxClearTask(taskHandle);

	}
	if (DAQmxFailed(error))
		printf("DAQmx Error: %s\n", errBuff);



	return 0;

}