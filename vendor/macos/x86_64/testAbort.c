#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "ftd3xx.h"

FT_HANDLE ftHandle = NULL;
BOOL bResult = TRUE;
int XFT_BASE_PIPE_IN = 0x82;
int XFT_BASE_PIPE_OUT = 0x02;

int XFT_REQ_PIPE_IN = 0x82;
int XFT_REQ_PIPE_OUT = 0x02;

bool device_open()
{
    FT_STATUS ftStatus = FT_OK;
    /*Open device by description*/
    ftStatus = FT_Create((PVOID)"FTDI SuperSpeed-FIFO Bridge",
                   FT_OPEN_BY_DESCRIPTION, &ftHandle);
    if (FT_FAILED(ftStatus))
    {
        printf("%s:%d FT_Create failed! %d \n",__FUNCTION__,__LINE__,ftStatus);
        return false;
    }
    return true;
}
FT_STATUS xft_write_pipe(FT_HANDLE ftHandle,
                         const UCHAR ucPipeID,
                         PUCHAR pucBuffer,
                         const ULONG ulBufferLength,
                         PULONG pulBytesTransferred)
{
    return FT_WritePipeEx(ftHandle, ucPipeID - XFT_BASE_PIPE_OUT, pucBuffer, ulBufferLength, pulBytesTransferred, 1000);
}
FT_STATUS xft_read_pipe_overlap(FT_HANDLE ftHandle,
                                const UCHAR ucPipeID,
                                PUCHAR pucBuffer,
                                const ULONG ulBufferLength,
                                PULONG pulBytesTransferred,
                                LPOVERLAPPED pOverlapped)
{
    FT_STATUS ftStatus = FT_OK;
    if(pOverlapped != NULL)
        ftStatus = FT_ReadPipeAsync(ftHandle, ucPipeID - XFT_BASE_PIPE_IN, pucBuffer, ulBufferLength, pulBytesTransferred, pOverlapped);
    
    return ftStatus;
}


bool testAbortPipeWithOverlapCompletedFreed()
{

    OVERLAPPED testOv = {};
    const FT_STATUS ftStatusInitOv = FT_InitializeOverlapped(ftHandle, &testOv);
    if (FT_FAILED(ftStatusInitOv))
    {
        printf("%s:%d FT_InitializeOverlapped failed! %d\n",__FUNCTION__,__LINE__,ftStatusInitOv);
        return false;
    }
    /*Write*/
    unsigned char cmdbuffer[4] = { 0xa, 0, 0, 0 };
    ULONG cmdBytesTx = 0;
    const FT_STATUS ftStatusWriteCmd = xft_write_pipe(ftHandle, XFT_REQ_PIPE_OUT,
                                                    cmdbuffer, sizeof (cmdbuffer), &cmdBytesTx);
    if(FT_FAILED(ftStatusWriteCmd))
    {
       printf("%s:%d writecmd failed!\n",__FUNCTION__,__LINE__);
        return false;
    }
    /*Read*/
    unsigned char respbuffer[4];
    ULONG bytesRx = 0;
    const FT_STATUS ftStatusReadOverlap = xft_read_pipe_overlap(ftHandle, XFT_REQ_PIPE_IN,
                                                                respbuffer, sizeof (respbuffer), &bytesRx, &testOv);
    if(!(ftStatusReadOverlap == FT_IO_PENDING)){
       printf("%s:%d readOverlap failed!\n",__FUNCTION__,__LINE__);
        return false;
    }

    const FT_STATUS ftStatusReadOvRes = FT_GetOverlappedResult(ftHandle, &testOv, &bytesRx, TRUE);
    if(FT_FAILED(ftStatusReadOvRes))
    {
       printf("%s:%d FT_GetOverlappedResult failed!\n",__FUNCTION__,__LINE__);
        return false;
    }
    const FT_STATUS ftStatusRelOv = FT_ReleaseOverlapped(ftHandle, &testOv);
    if(FT_FAILED(ftStatusRelOv)){
       printf("%s:%d FT_ReleaseOverlapped failed!\n",__FUNCTION__,__LINE__);
        return false;       
    }
    const FT_STATUS ftStatusab = FT_AbortPipe(ftHandle, XFT_REQ_PIPE_IN);
    const bool ok_status = (ftStatusab == FT_OK);
     printf("%s:%d %s - abortpipe with overlap completed freed result (expected=FT_OK=%d actual=%d)\n",__FUNCTION__,__LINE__,
           ok_status ? "PASS" : "FAIL", (int)FT_OK, (int)ftStatusab);


    return ok_status;
}


bool testAbortPipeWithOverlapCompleted()
{
    OVERLAPPED testOv = {};
    const FT_STATUS ftStatusInitOv = FT_InitializeOverlapped(ftHandle, &testOv);
    if(FT_FAILED(ftStatusInitOv))
    {
       printf("%s:%d FT_InitializeOverlapped failed! %d\n",__FUNCTION__,__LINE__,ftStatusInitOv);
        return false;
    }
    /*Write*/
    unsigned char cmdbuffer[4] = { 0xa, 0, 0, 0 };
    ULONG cmdBytesTx = 0;
    const FT_STATUS ftStatusWriteCmd = xft_write_pipe(ftHandle, XFT_REQ_PIPE_OUT,
                                                    cmdbuffer, sizeof (cmdbuffer), &cmdBytesTx);
    if(FT_FAILED(ftStatusWriteCmd))
    {
       printf("%s:%d writecmd failed!\n",__FUNCTION__,__LINE__);
        return false;
    }
    /*Read*/
    unsigned char respbuffer[4];
    ULONG bytesRx = 0;
    const FT_STATUS ftStatusReadOverlap = xft_read_pipe_overlap(ftHandle, XFT_REQ_PIPE_IN,
                                                                respbuffer, sizeof (respbuffer), &bytesRx, &testOv);
    if(!(ftStatusReadOverlap == FT_IO_PENDING)){
       printf("%s:%d readOverlap failed!\n",__FUNCTION__,__LINE__);
        return false;
    }

    const FT_STATUS ftStatusReadOvRes = FT_GetOverlappedResult(ftHandle, &testOv, &bytesRx, TRUE);
    if(FT_FAILED(ftStatusReadOvRes))
    {
       printf("%s:%d FT_GetOverlappedResult failed!\n",__FUNCTION__,__LINE__);
        return false;
    }

    const FT_STATUS ftStatusab = FT_AbortPipe(ftHandle, XFT_REQ_PIPE_IN);
    const bool ok_status = (ftStatusab == FT_OK);
     printf("%s:%d: \n===============================\n %s - abortpipe with overlap completed freed result (expected=FT_OK=%d actual=%d)\n",__FUNCTION__,__LINE__,
           ok_status ? "PASS" : "FAIL", (int)FT_OK, (int)ftStatusab);



    const FT_STATUS ftStatusRelOv = FT_ReleaseOverlapped(ftHandle, &testOv);
    if(FT_FAILED(ftStatusRelOv)){
       printf("%s:%d FT_ReleaseOverlapped failed!\n",__FUNCTION__,__LINE__);
        return false;       
    }

    return ok_status;
}




int main()
{
    if(device_open())
    {
        bool ok = true;
        ok &= testAbortPipeWithOverlapCompletedFreed();
        ok &= testAbortPipeWithOverlapCompleted();
        printf("\n\n=====================\nOverall abortpipe tests result:%s \n========================== \n", ok ? "PASS -" : "FAIL -");

    }

   
    return 0;
}