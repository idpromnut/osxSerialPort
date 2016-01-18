#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <paths.h>
#include <termios.h>
#include <sysexits.h>
#include <sys/param.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>

#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/IOBSD.h>




static kern_return_t findModems(io_iterator_t *matchingServices)
{
    kern_return_t       kernResult;
    mach_port_t         masterPort;
    CFMutableDictionaryRef  classesToMatch;
    
    kernResult = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if (KERN_SUCCESS != kernResult)
    {
        printf("IOMasterPort returned %d\n", kernResult);
        goto exit;
    }
    
    // Serial devices are instances of class IOSerialBSDClient.
    classesToMatch = IOServiceMatching(kIOSerialBSDServiceValue);
    if (classesToMatch == NULL)
    {
        printf("IOServiceMatching returned a NULL dictionary.\n");
    }
    else {
        CFDictionarySetValue(classesToMatch,
                             CFSTR(kIOSerialBSDTypeKey),
                             CFSTR(kIOSerialBSDRS232Type));
        // Each serial device object has a property with key
        // kIOSerialBSDTypeKey and a value that is one of
        // kIOSerialBSDAllTypes, kIOSerialBSDModemType,
        // or kIOSerialBSDRS232Type. You can change the
        // matching dictionary to find other types of serial
        // devices by changing the last parameter in the above call
        // to CFDictionarySetValue.
    }
    
    kernResult = IOServiceGetMatchingServices(masterPort, classesToMatch, matchingServices);
    
    if (KERN_SUCCESS != kernResult)
    {
        printf("IOServiceGetMatchingServices returned %d\n", kernResult);
        goto exit;
    }

exit:
    return kernResult;
}

static kern_return_t getModemPath(io_iterator_t serialPortIterator, char *bsdPath, CFIndex maxPathSize)
{
    io_object_t     modemService;
    kern_return_t   kernResult = KERN_FAILURE;
    Boolean         modemFound = false;
    
    // Initialize the returned path
    *bsdPath = '\0';
    
    // Iterate across all modems found. In this example, we bail after finding the first modem.
    while ((modemService = IOIteratorNext(serialPortIterator)) && !modemFound) {
        CFTypeRef   bsdPathAsCFString;
        
        // Get the callout device's path (/dev/cu.xxxxx). The callout device should almost always be
        // used: the dialin device (/dev/tty.xxxxx) would be used when monitoring a serial port for
        // incoming calls, e.g. a fax listener.
        bsdPathAsCFString = IORegistryEntryCreateCFProperty(modemService,
                                                            CFSTR(kIODialinDeviceKey),
                                                            kCFAllocatorDefault,
                                                            0);
        if (bsdPathAsCFString) {
            Boolean result;
            
            // Convert the path from a CFString to a C (NUL-terminated) string for use
            // with the POSIX open() call.
            result = CFStringGetCString(bsdPathAsCFString,
                                        bsdPath,
                                        maxPathSize,
                                        kCFStringEncodingUTF8);
            CFRelease(bsdPathAsCFString);
            
#ifdef MATCH_PATH
            if (strncmp(bsdPath, MATCH_PATH, strlen(MATCH_PATH)) != 0) {
                result = false;
            }
#endif
            if (result) {
                printf("Modem found with BSD path: %s", bsdPath);
                modemFound = true;
                kernResult = KERN_SUCCESS;
            }
        }
        
        printf("\n");
        
        // Release the io_service_t now that we are done with it.
        (void) IOObjectRelease(modemService);
    }
    
    return kernResult;
}


int main(void) {

//    int             fileDescriptor;
    kern_return_t   kernResult;
    io_iterator_t   serialPortIterator;
    char            deviceFilePath[MAXPATHLEN];
    
    kernResult = findModems(&serialPortIterator);

    while(kernResult == KERN_SUCCESS) {
        kernResult = getModemPath(serialPortIterator, deviceFilePath,
                                  sizeof(deviceFilePath));
    }
    
    IOObjectRelease(serialPortIterator);    // Release the iterator.
    
    // Open the modem port, initialize the modem, then close it.
    if (!deviceFilePath[0])
    {
        printf("No modem port found.\n");
        return EX_UNAVAILABLE;
    }

}