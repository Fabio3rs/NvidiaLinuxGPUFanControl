// https://github.com/Fabio3rs
// This code needs improvements
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <memory>

class unique_file
{
    FILE *fp;

public:
    FILE *get() const { return fp; }
    void reset() { if (fp) fclose(fp); fp = nullptr; }
	
    operator bool() const { return fp != nullptr; }
    
    unique_file &operator=(const unique_file&) = delete;
	
    unique_file &operator=(unique_file &&u)
	{
		if (std::addressof(u) == this) return *this;
		reset();
		fp = std::move(u.fp); u.fp = nullptr; return *this;
	}
    
    unique_file() : fp(nullptr) {}
    unique_file(FILE *p) : fp(p) {}
    unique_file(unique_file &&u) : fp(std::move(u.fp)) { u.fp = nullptr; }
    ~unique_file() { if (fp) fclose(fp); }
};

std::string getNvidiaAttribute(const char *attr)
{
    unique_file u(popen((std::string("/usr/bin/nvidia-settings -q ") + attr).c_str(), "r"));

    if (u)
    {
        char path[1280];        
        
        while (fgets(path, sizeof(path)-1, u.get()) != NULL) {
            char *o = strstr(path, "): ");
            if (o) return std::string(o + sizeof("): ") - 1);
        }
        return "Error";
    }else
    {
        printf("Failed to run command\n" );
        return "";
    }
}

void setNvidiaAttribute(const char *attr, const char *value)
{
    std::string result = "/usr/bin/nvidia-settings -a ";
    result += attr;
    result += "=";
    result += value;
    
    unique_file u(popen(result.c_str(), "r"));
    
    if (u)
    {
    }else
    {
        printf("Failed to run command\n" );
    } 
}

const int minspeed = 30;
const int mintemp = 20;

const int maxspeed = 100;
const int maxtemp = 85;

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

int main()
{
    printf("%s\n", getNvidiaAttribute("[gpu:0]/GPUCoreTemp").c_str());
    printf("%s\n", getNvidiaAttribute("[gpu:0]/GPUFanControlState").c_str());
    printf("%s\n", getNvidiaAttribute("[fan:0]/GPUTargetFanSpeed").c_str());
    
    setNvidiaAttribute("[gpu:0]/GPUFanControlState", "1");
        
    const float deltaTemp = maxtemp - mintemp;
    const float deltaSpeed = maxspeed - minspeed;
    
    float speedTemp = deltaSpeed / deltaTemp;       
    
    while (true)
    {
        int GPUTemp = strtoint(getNvidiaAttribute("[gpu:0]/GPUCoreTemp"), 0);
        
        if (GPUTemp < mintemp)
        {
            setNvidiaAttribute("[fan:0]/GPUTargetFanSpeed", std::to_string(minspeed).c_str());
        }else if (GPUTemp >= maxtemp)
        {
            setNvidiaAttribute("[fan:0]/GPUTargetFanSpeed", std::to_string(maxspeed).c_str());
        }else
        {
            float dtemp = (GPUTemp - mintemp);
            float targetspeed = deltaSpeed;            
            dtemp /= deltaTemp;
            targetspeed *= dtemp;
            targetspeed += minspeed;
            
            printf("%d %f %f\n", GPUTemp, dtemp, targetspeed);
            setNvidiaAttribute("[fan:0]/GPUTargetFanSpeed", std::to_string((int)targetspeed).c_str());
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}


