// https://github.com/Fabio3rs
// This code needs improvements
// Use at your own risk
// Use por sua conta e risco.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <memory>
#include <utility>
#include <vector>

// For sigaction
#include <signal.h>
#include <unistd.h>


// RAII for FILE*
class CFiledtor
{
public:
	void operator() (FILE *p) const { fclose(p); }
};

typedef std::unique_ptr<FILE, CFiledtor> unique_file;


typedef std::vector<std::pair<std::string, std::string>> command_t;

std::string getNvidiaAttribute(const char *attr)
{
	char buff[1024] = { "/usr/bin/nvidia-settings -q " };
	strcat(buff, attr);

    unique_file u(popen(buff, "r"));

    if (u)
    {
        char path[1280];        
        
        while (fgets(path, sizeof(path)-1, u.get()) != nullptr) {
            char *o = strstr(path, "): ");
            if (o) return std::string(o + sizeof("): ") - 1);
        }
        return "";
    }else
    {
        printf("Failed to run command\n" );
        return "Error";
    }
}

void setNvidiaAttributes(const command_t &commands)
{
	if (commands.size() == 0)
		return;

	std::string commandLine = { "/usr/bin/nvidia-settings -a" };

	for (auto &c : commands) { commandLine += " "; commandLine += c.first + "=" + c.second; }

	unique_file u(popen(commandLine.c_str(), "r"));

	if (u)
	{   
        char buff[1280];        
        
        while (fgets(buff, sizeof(buff) - 1, u.get()) != nullptr) {
            char *o = strstr(buff, "ERROR");
            if (o) printf("%s\n", buff);
        }
	}
	else
	{
		printf("Failed to run command\n");
	}
}

void setNvidiaAttribute(const std::string &attribute, const std::string &value)
{
	setNvidiaAttributes({ {attribute, value} });
}

int strtoint(const std::string &str, int defaultValue)
{
    try {
        return std::stoi(str);
    }catch(const std::exception &e)
    {
        printf("%s", e.what());
    }
    return defaultValue;
}

void atexitThings(int s)
{
    printf("\n");

    printf("Setting GPUFanControlState to 0...\n");
    setNvidiaAttribute("[gpu:0]/GPUFanControlState", "0");
    
    printf("Exiting\n");
    exit(1);
}

void catchExitSignal()
{
    // https://stackoverflow.com/questions/1641182/how-can-i-catch-a-ctrl-c-event
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = atexitThings;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);
}

// TODO: Read the data from file
// TODO: Possibility to create a speed curve
// Data to calculate fan speed
const int minspeed = 30;
const int mintemp = 20;

const int maxspeed = 100;
const int maxtemp = 85;

int main()
{
    int GPUTemp = strtoint(getNvidiaAttribute("[gpu:0]/GPUCoreTemp"), 0);
    printf("[gpu:0]/GPUCoreTemp         %d\n", GPUTemp);
    printf("[gpu:0]/GPUFanControlState  %s\n", getNvidiaAttribute("[gpu:0]/GPUFanControlState").c_str());
    printf("[fan:0]/GPUTargetFanSpeed   %s\n", getNvidiaAttribute("[fan:0]/GPUTargetFanSpeed").c_str());
    
    printf("Setting GPUFanControlState to 1...\n");
    setNvidiaAttribute("[gpu:0]/GPUFanControlState", "1");
    
    // handle Ctrl C
    catchExitSignal();
    
    const float deltaTemp = maxtemp - mintemp;
    const float deltaSpeed = maxspeed - minspeed;
    
    float speedTemp = deltaSpeed / deltaTemp;       

    int lastGPUCoreTemp = GPUTemp + 1; // Make sure the fan speed calculations will run at least once
    
    printf("GPU Temperature     Target fan speed\n");
    while (true)
    {
        GPUTemp = strtoint(getNvidiaAttribute("[gpu:0]/GPUCoreTemp"), 0);
        
        // Optimization to reduce the number of calls to nvidia-settings
        if (lastGPUCoreTemp != GPUTemp)
        {
            if (GPUTemp < mintemp)
            {
                printf("%d                  %d\n", GPUTemp, minspeed);
                setNvidiaAttribute("[fan:0]/GPUTargetFanSpeed", std::to_string(minspeed).c_str());
            }else if (GPUTemp >= maxtemp)
            {
                printf("%d                  %d\n", GPUTemp, maxspeed);
                setNvidiaAttribute("[fan:0]/GPUTargetFanSpeed", std::to_string(maxspeed).c_str());
            }else
            {
                float dtemp = (GPUTemp - mintemp);
                float targetspeed = deltaSpeed;            
                dtemp /= deltaTemp;
                targetspeed *= dtemp;
                targetspeed += minspeed;
                
                int percentfanspeed = targetspeed;
                
                printf("%d                  %d\n", GPUTemp, percentfanspeed);
                setNvidiaAttribute("[fan:0]/GPUTargetFanSpeed", std::to_string(percentfanspeed));
            }
            lastGPUCoreTemp = GPUTemp;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    return 0;
}


