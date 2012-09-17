#include <IOKit/IOKitLib.h>
#include <ApplicationServices/ApplicationServices.h>
#include <IOKit/i2c/IOI2CInterface.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* ----------------------------------------------------- */

void ddcSet(int code, int value)
{
	kern_return_t	kr; 
	io_service_t	framebuffer, interface;
	IOOptionBits	bus;
	IOItemCount		busCount;

	framebuffer = CGDisplayIOServicePort(CGMainDisplayID());

	io_string_t path; 
	kr = IORegistryEntryGetPath(framebuffer, kIOServicePlane, path);
	assert( KERN_SUCCESS == kr );

	kr = IOFBGetI2CInterfaceCount( framebuffer, &busCount ); 
	assert( kIOReturnSuccess == kr );

	for( bus = 0; bus < busCount; bus++ )
	{
		IOI2CConnectRef	 connect;

		kr = IOFBCopyI2CInterfaceForBus(framebuffer, bus, &interface); 
		
		if( kIOReturnSuccess != kr)
			continue;

		kr = IOI2CInterfaceOpen( interface, kNilOptions, &connect );

		IOObjectRelease(interface); 
		assert( kIOReturnSuccess == kr );
		
		if( kIOReturnSuccess != kr)
			continue;

		kern_return_t kr;
		IOI2CRequest	request;
		UInt8			data[128];

		bzero( &request, sizeof(request)); 

		request.commFlags						= 0;

		request.sendAddress						= 0x6E;
		request.sendTransactionType				= kIOI2CSimpleTransactionType;
		request.sendBuffer						= (vm_address_t) &data[0];
		request.sendBytes						= 7;

		data[0] = 0x51;		// Sub-address
		data[1] = 0x84;		
		data[2] = 0x03;
		data[3] = code;		// VCP code
		data[4] = 0x64;		// Max value
		data[5] = value;	// New value
		data[6] = 0x6E ^ data[0] ^ data[1] ^ data[2] ^ data[3]^ data[4]^ data[5];

		request.replyTransactionType	= kIOI2CNoTransactionType;
		request.replyBytes				= 0; //128; 

		kr = IOI2CSendRequest( connect, kNilOptions, &request );
		assert( kIOReturnSuccess == kr );
	
		if( kIOReturnSuccess != request.result)
			return;

		IOI2CInterfaceClose( connect, kNilOptions ); 
	}
}

/* ----------------------------------------------------- */

void ddcGet(int code, UInt8 *inData)
{
	kern_return_t kr;
	io_service_t framebuffer, interface;
	IOOptionBits bus;
	IOItemCount busCount;

	framebuffer = CGDisplayIOServicePort(CGMainDisplayID());

	io_string_t path;
	kr = IORegistryEntryGetPath(framebuffer, kIOServicePlane, path);
	assert( KERN_SUCCESS == kr );

	kr = IOFBGetI2CInterfaceCount( framebuffer, &busCount );
	assert( kIOReturnSuccess == kr );
	
	for( bus = 0; bus < busCount; bus++ )
	{
		IOI2CConnectRef connect;

		kr = IOFBCopyI2CInterfaceForBus(framebuffer, bus, &interface);
		
		if( kIOReturnSuccess != kr)
			continue;

		kr = IOI2CInterfaceOpen( interface, kNilOptions, &connect );

		IOObjectRelease(interface);
		assert( kIOReturnSuccess == kr );
		
		if( kIOReturnSuccess != kr)
			continue;

		kern_return_t kr;
		IOI2CRequest request;
		UInt8 data[128];

		bzero( &request, sizeof(request));

		request.commFlags = 0;

		request.sendAddress = 0x6E;
		request.sendTransactionType = kIOI2CSimpleTransactionType;
		request.sendBuffer = (vm_address_t) &data[0];
		request.sendBytes = 5;
		request.minReplyDelay = 6000000;

		data[0] = 0x51;
		data[1] = 0x82;
		data[2] = 0x01;
		data[3] = code;
		data[4] = 0x6E ^ data[0] ^ data[1] ^ data[2] ^ data[3];

		request.replyTransactionType = kIOI2CDDCciReplyTransactionType;
		request.replyAddress = 0x6F;
		request.replySubAddress = 0x51;
		request.replyBuffer = (vm_address_t) &inData[0];
		request.replyBytes = 20;
		bzero( &inData[0], request.replyBytes );

		kr = IOI2CSendRequest( connect, kNilOptions, &request );
		assert( kIOReturnSuccess == kr );

		IOI2CInterfaceClose( connect, kNilOptions );
	}
}

/* ----------------------------------------------------- */

int main( int argc, char * argv[] )
{
	if (argc == 2)
	{
		int code = atoi(argv[1]);
		
		UInt8 inData[20];
		ddcGet(code, &inData[0]);
		
		printf("Maximum: %i\n", inData[7]);
		printf("Current: %i\n", inData[9]);
	}
	else if (argc == 3)
	{
		int code = atoi(argv[1]);
		int value = atoi(argv[2]);
		
		char c = argv[2][0];
		
		if ( c == '+' || c == '-' )
		{
			UInt8 inData[20];
			ddcGet(code, &inData[0]);
		
			value += inData[9];
			value = (value > inData[7]) ? inData[7] : (value < 0) ? 0 : value;
		}
		
		ddcSet(code, value);
	}

	exit(0);
	return(0);
}