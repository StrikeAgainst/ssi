#include "Serial.h"
#include "SSI_Cons.h"

//uncomment this to see the raw bytes received and sent on std::cout
//#define SERIAL_DEBUG_TO_CONSOLE

#ifdef SERIAL_DEBUG_TO_CONSOLE
#include <iostream>
#include <iomanip>
#endif // SERIAL_DEBUG_TO_CONSOLE


Serial::Serial(const char *portName, DWORD speed )
{
    this->connected = false;

	char com [100];
	_snprintf(com, 100, "\\\\.\\%s", portName);	

	this->hSerial = CreateFile( com,
        GENERIC_READ | GENERIC_WRITE,
        0,    // exclusive access 
        NULL, // default security attributes 
        OPEN_EXISTING,
        0,//FILE_FLAG_OVERLAPPED,
        NULL 
    );

    if (hSerial == INVALID_HANDLE_VALUE) 
    {        
        printf("CreateFile failed with error %d.\n", GetLastError());
        return;
    }

    //Check if the connection was successfull
    if(this->hSerial==INVALID_HANDLE_VALUE)
    {
        //If not success full display an Error
        if(GetLastError() == ERROR_FILE_NOT_FOUND)
            printf("ERROR: Handle was not attached. Reason: %s not available.\n", portName);
        else
            printf("ERROR!!!");
    }
    else
    {
        //If connected we try to set the comm parameters
        DCB dcbSerialParams = {0};

        //Try to get the current
        if (!GetCommState(this->hSerial, &dcbSerialParams))
        {
            printf("failed to get current serial parameters!");
        }
        else
        {
            dcbSerialParams.BaudRate = speed;
            dcbSerialParams.ByteSize = 8;
            dcbSerialParams.StopBits = ONESTOPBIT;
            dcbSerialParams.Parity = NOPARITY;

            //Set the parameters and check for their proper application
            if(!SetCommState(hSerial, &dcbSerialParams))
				printf("ALERT: Could not set Serial Port parameters");
            else            
                this->connected = true;

            //timeouts are set as short as possible to ensure that real-time is enforced, even if it means dropping data
            //do not remove this, as it seems to lead to WriteFile blocking indefinetely on a bluetooth connection
            COMMTIMEOUTS timeouts;
            timeouts.ReadIntervalTimeout = 1;
            timeouts.ReadTotalTimeoutMultiplier = 1;
            timeouts.ReadTotalTimeoutConstant = 1;
            timeouts.WriteTotalTimeoutMultiplier = 1;
            timeouts.WriteTotalTimeoutConstant = 1;
            if (!SetCommTimeouts(hSerial, &timeouts))
                printf("Setting COMMTIMEOUTS failed");
		}
    }

}

Serial::~Serial()
{
    if(this->connected)
    {
        this->connected = false;
        CloseHandle(this->hSerial);
    }
}

int Serial::ReadData(unsigned char *buffer, unsigned int nbChar)
{
    //Number of bytes we'll have read
    DWORD bytesRead;
    //Number of bytes we'll really ask to read
    unsigned int toRead = nbChar;
    //Use the ClearCommError function to get status info on the Serial port
    ClearCommError(this->hSerial, &this->errors, &this->status);

    while(toRead > 0)
    {
        if(!ReadFile(this->hSerial, buffer + (nbChar - toRead), toRead, &bytesRead, NULL))
        {
            return -1;
        }

		toRead -= bytesRead;
    }

#ifdef SERIAL_DEBUG_TO_CONSOLE
    std::cout << "Received over Serial: " << std::endl;
    for (size_t i = 0; i < nbChar; i++) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(*(buffer + i)) << " ";
    }
    std::cout << std::endl;
#endif // SERIAL_DEBUG_TO_CONSOLE

    return nbChar;
}


bool Serial::WriteData(unsigned char *buffer, unsigned int nbChar)
{
    DWORD bytesSend;

    //Try to write the buffer on the Serial port
    if(!WriteFile(this->hSerial, buffer, nbChar, &bytesSend, NULL)) 
    {
		printf("Erorr during serial write: %X\n", GetLastError());
        //In case it don't work get comm error and return false
        ClearCommError(this->hSerial, &this->errors, &this->status);
        return false;
    }

#ifdef SERIAL_DEBUG_TO_CONSOLE
    std::cout << "Sent over Serial: " << std::endl;
    for (size_t i = 0; i < nbChar; i++) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(*(buffer + i)) << " ";
    }
    std::cout << std::endl;
#endif // SERIAL_DEBUG_TO_CONSOLE    
    
    return true;
}

bool Serial::IsConnected()
{
    return this->connected;
}

