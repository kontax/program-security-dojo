#define _GNU_SOURCE 1

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/sendfile.h>
#include <sys/prctl.h>
#include <sys/personality.h>
#include <arpa/inet.h>

#include <capstone/capstone.h>

#define CAPSTONE_ARCH CS_ARCH_X86
#define CAPSTONE_MODE CS_MODE_64

void print_disassembly(void *shellcode_addr, size_t shellcode_size)
{
    csh handle;
    cs_insn *insn;
    size_t count;

    if (cs_open(CAPSTONE_ARCH, CAPSTONE_MODE, &handle) != CS_ERR_OK)
    {
        printf("ERROR: disassembler failed to initialize.\n");
        return;
    }

    count = cs_disasm(handle, shellcode_addr, shellcode_size, (uint64_t)shellcode_addr, 0, &insn);
    if (count > 0)
    {
        size_t j;
        printf("      Address      |                      Bytes                    |          Instructions\n");
        printf("------------------------------------------------------------------------------------------\n");

        for (j = 0; j < count; j++)
        {
            printf("0x%016lx | ", (unsigned long)insn[j].address);
            for (int k = 0; k < insn[j].size; k++) printf("%02hhx ", insn[j].bytes[k]);
            for (int k = insn[j].size; k < 15; k++) printf("   ");
            printf(" | %s %s\n", insn[j].mnemonic, insn[j].op_str);
        }

        cs_free(insn, count);
    }
    else
    {
        printf("ERROR: Failed to disassemble shellcode! Bytes are:\n\n");
        printf("      Address      |                      Bytes\n");
        printf("--------------------------------------------------------------------\n");
        for (unsigned int i = 0; i <= shellcode_size; i += 16)
        {
            printf("0x%016lx | ", (unsigned long)shellcode_addr+i);
            for (int k = 0; k < 16; k++) printf("%02hhx ", ((uint8_t*)shellcode_addr)[i+k]);
            printf("\n");
        }
    }

    cs_close(&handle);
}

void *shellcode;
size_t shellcode_size;

int main(int argc, char **argv, char **envp)
{
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("###\n");
    printf("### Welcome to %s!\n", argv[0]);
    printf("###\n");
    printf("\n");

    puts("This challenge reads in some bytes, modifies them (depending on the specific challenge configuration), and executes them");
    puts("as code! This is a common exploitation scenario, called `code injection`. Through this series of challenges, you will");
    puts("practice your shellcode writing skills under various constraints! To ensure that you are shellcoding, rather than doing");
    puts("other tricks, this will sanitize all environment variables and arguments and close all file descriptors > 2.\n");
    for (int i = 3; i < 10000; i++) close(i);
    for (char **a = argv; *a != NULL; a++) memset(*a, 0, strlen(*a));
    for (char **a = envp; *a != NULL; a++) memset(*a, 0, strlen(*a));

    shellcode = mmap((void *)0x14243000, 0x1000, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_ANON, 0, 0);
    assert(shellcode == (void *)0x14243000);
    printf("Mapped 0x1000 bytes for shellcode at %p!\n", shellcode);

    puts("Reading 0x1000 bytes from stdin.\n");
    shellcode_size = read(0, shellcode, 0x1000);
    assert(shellcode_size > 0);

    puts("Executing filter...\n");
    uint64_t *input = shellcode;
    int sort_max = shellcode_size / sizeof(uint64_t) - 1;
    for (int i = 0; i < sort_max; i++)
        for (int j = 0; j < sort_max-i-1; j++)
            if (input[j] > input[j+1])
            {
                uint64_t x = input[j];
                uint64_t y = input[j+1];
                input[j] = y;
                input[j+1] = x;
            }
    puts("This challenge just sorted your shellcode using bubblesort. Keep in mind the impact of memory endianness on this sort");
    puts("(e.g., the LSB being the right-most byte).\n");
    printf("This sort processed your shellcode %d bytes at a time.\n", sizeof(uint64_t));

    puts("This challenge is about to execute the following shellcode:\n");
    print_disassembly(shellcode, shellcode_size);
    puts("");

    puts("This challenge is about to close stdin, which means that it will be harder to pass in a stage-2 shellcode. You will need");
    puts("to figure an alternate solution (such as unpacking shellcode in memory) to get past complex filters.\n");
    assert(fclose(stdin) == 0);

    puts("Executing shellcode!\n");
    ((void(*)())shellcode)();

    printf("### Goodbye!\n");
}