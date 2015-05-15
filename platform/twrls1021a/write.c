#include <unistd.h>


typedef unsigned char B;

#define OUTB(x,c) out_b((volatile B*)x,c)
#define INB(x) in_b((volatile B*)x)

#define	DUARTxBase(x)		(0x21c0500 + 0x001000*(x-1))
#define DUARTx_URBRn(x,n)		(DUARTxBase(x) + 0x100*(n-1) + 0x00)
#define DUARTx_UTHRn(x,n)		(DUARTxBase(x) + 0x100*(n-1) + 0x00)
#define DUARTx_UDLBn(x,n)		(DUARTxBase(x) + 0x100*(n-1) + 0x00)
#define DUARTx_UDMBn(x,n)		(DUARTxBase(x) + 0x100*(n-1) + 0x01)
#define DUARTx_UIERn(x,n)		(DUARTxBase(x) + 0x100*(n-1) + 0x01)
#define DUARTx_UIIRn(x,n)		(DUARTxBase(x) + 0x100*(n-1) + 0x02)
#define DUARTx_UFCRn(x,n)		(DUARTxBase(x) + 0x100*(n-1) + 0x02)
#define DUARTx_UAFRn(x,n)		(DUARTxBase(x) + 0x100*(n-1) + 0x02)
#define DUARTx_ULCRn(x,n)		(DUARTxBase(x) + 0x100*(n-1) + 0x03)
#define DUARTx_UMCRn(x,n)		(DUARTxBase(x) + 0x100*(n-1) + 0x04)
#define DUARTx_ULSRn(x,n)		(DUARTxBase(x) + 0x100*(n-1) + 0x05)
#define DUARTx_UMSRn(x,n)		(DUARTxBase(x) + 0x100*(n-1) + 0x06)
#define DUARTx_USCRn(x,n)		(DUARTxBase(x) + 0x100*(n-1) + 0x07)
#define DUARTx_UDSRn(x,n)		(DUARTxBase(x) + 0x100*(n-1) + 0x10)

volatile B in_b(volatile void *addr){
	return *(volatile B *)addr;
}

volatile void out_b(volatile void *addr, B val){
	*(volatile B *)addr = val;
}

ssize_t
write(int fd, const void *buf, size_t count)
{
	const char *cbuf = buf;
	int i;

	for (i = 0; i < count; i++){
		if(cbuf[i]=='\n'){
			while((INB(DUARTx_UDSRn(1,1))&0x2)!=0);
			OUTB(DUARTx_UTHRn(1,1), '\r');
		}
		while((INB(DUARTx_UDSRn(1,1))&0x2)!=0);
		OUTB(DUARTx_UTHRn(1,1), cbuf[i]);
	}

	return count;
}
