/*
 * Debug helper functions.
 */
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/time.h>

#if defined(__GNUC__) && ((__GNUC__ < 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ < 3)))
/* Consider any GCC version older than 4.3 as 'old'. */
#define OLD_GCC 1
#endif

/* Android pthread.h doesn't have getname_np and setname_np */
#if defined(_GNU_SOURCE) && !defined(__ANDROID__) && !defined(__UCLIBC__) && !defined(OLD_GCC)
  #define PTHREAD_NAMING_AVAILABLE
  #include <pthread.h>
#endif
#include "debug.h"

/* If debug output is often truncated, increase BUF_SIZE. */
#define BUF_SIZE 256
/* Time-stamp as seconds-mod-60, microseconds, space e.g. "42.123456 " */
#define TIMESTAMP_SIZE 10
/* Typically, pthread limits thread names to 15 chars, plus '\0' */
#define THREAD_NAME_SIZE 16


static DebugLevel currentLevel = DebugLevel_All;



void Debug_setLevel(DebugLevel level)
{
    currentLevel = level;
}



/* Print debug message, prefixed by thread name.
 * The fixed-size buffer here might truncate the original message. 
 */
void Debug_printf(DebugLevel level, const char *format, ...)
{
    static const char  unknownName[] = "UnknownThread";
    char               buf[BUF_SIZE];
    char              *cursor = buf;
    struct timeval     now;
    int                padding = 0;
    int                remainingChars = BUF_SIZE;
    va_list            args;
    int                f;
    int                leadingNewlines = 0;

    if (currentLevel < level)
    {
        /* This message is not required at the current debug level. */
        return;
    }

    gettimeofday(&now, NULL);
    (void)snprintf(cursor,
                   remainingChars,
                   "%02d.%06d ", 
                   (int)now.tv_sec % 60,
                   (int)now.tv_usec);

    cursor += TIMESTAMP_SIZE;
    remainingChars -= TIMESTAMP_SIZE;
    *cursor = '\0';

#ifdef PTHREAD_NAMING_AVAILABLE
    if (0 == pthread_getname_np(pthread_self(), 
                                cursor, 
                                THREAD_NAME_SIZE) 
        && cursor[0] != '\0')
    {
        /* Name of current thread is now at the start of buf. */
    }
    else
#endif /* PTHREAD_NAMING_AVAILABLE */
    {
        strcpy(cursor, unknownName);
    }

    /* Append spaces to thread name so it occupies THREAD_NAME_SIZE chars */
    for (f = 0; f < THREAD_NAME_SIZE; f++)
    {
        if (padding == 0 && cursor[f] == '\0')
            padding = 1;
            
        if (padding == 1)
            cursor[f] = ' ';
    }
    
    /* Thread name is always zero-terminated so we're guaranteed to have
     * inserted at least one space.
     */
    assert(padding == 1);

    cursor += THREAD_NAME_SIZE;
    remainingChars -= THREAD_NAME_SIZE;
    
    /* Now print the rest of the message into buf */
    va_start(args, format);
    (void)vsnprintf(cursor,
                    remainingChars,
                    format, 
                    args);
    va_end(args);

    /* Count leading newlines in the original message and replace
     * them with spaces, to keep thread name on same line as message.
     */
    for (f = 0; f < remainingChars; f++)
    {
        if (cursor[f] == '\n')
        {
            leadingNewlines++;
            cursor[f] = ' ';
        }
        else
        {
            /* This (and any subsequent character) is not a leading newline */
            break;
        }
    }
    
    /* Honour the leading newlines */
    for (f = 0; f < leadingNewlines; f++)
    {
        printf("\n");
    }
        
    (void)printf("%s", buf);
    
    fflush(stdout);
}



/* Print pre-formatted debug message. */
void Debug_printMessage(const char *message)
{
    Debug_printf(DebugLevel_All, "%s", message);
}



void Debug_nameThread(const char *name, unsigned int *id)
{
#ifndef PTHREAD_NAMING_AVAILABLE
    (void)name;
    (void)id;
    return;
#else
    char          threadName[16]; /* Typical limit for thread name */
    unsigned int  appendix = 0;
    
    assert(name != NULL);
    
    if (id != NULL)
    {
        if (*id >= 0xFFFFFFFF)
        {
            /* Unlikely, but keep following code safe with 8 character limit */
            appendix = 0xFFFFFFFF;
        }
        else
        {
            appendix = *id;
        }
    
        /* Print at most 6 characters of name, plus appendix as hex */
        (void)snprintf(threadName, sizeof(threadName), 
                       "%.6s-%08x", name, appendix);
    }
    else
    {
        (void)strncpy(threadName, name, sizeof(threadName));
        threadName[sizeof(threadName)-1] = '\0';
    }

    if (0 != pthread_setname_np(
#ifndef _OSX_
                 pthread_self(), /* OS X API doesn't have this parameter */
#endif /* _OSX_ */
                 threadName))
    {
        Debug_printf(DebugLevel_Warn,
                     "Failed to name thread %s.\n",
                     name);
    }
#endif /* PTHREAD_NAMING_AVAILABLE */
}



void Debug_hexdump(DebugLevel   level,
                   void        *address, 
                   size_t       length)
{
    char      buf[3*8 + 2 + 8 + 1];
    char      subString[4];
    size_t    f;
    char      unprinted = 0;

    if (currentLevel < level)
    {
        /* This message is not required at the current debug level. */
        return;
    }

    Debug_printf(level, "Hex-dump of %p:\n", address);

    buf[0] = '\0';

    for (f = 0; f < length; f++)
    {
        unsigned char thisByte;
        char          printable;
        int           offsetInLine = f % 8;

        if (offsetInLine == 0)
        {
            // New line.  Display previous line...
            printf("%s\n%p: ", buf, address + f);
            // And next line
            //Debug_printf(level, "%p: ", address + f);
            unprinted = 0;
            // ...and clear buffer ready for the new line.
            memset(buf, (int)' ', sizeof(buf));
            buf[sizeof(buf) - 1] = '\0';
        }

        thisByte = ((unsigned char *)address)[f];

        sprintf(subString, "%02x ", (unsigned int)thisByte);        
        memcpy(&buf[offsetInLine * 3], subString, 3);

        if ( isprint((int)thisByte) )
            printable = (char)thisByte;
        else
            printable = '.';
        sprintf(subString, "%c", printable);
        memcpy(&buf[3*8 + 2 + offsetInLine], subString, 1);        

        unprinted++; // Remember 
    }

    if (unprinted)
        printf("%s\n", buf);

    printf("\n");
}
