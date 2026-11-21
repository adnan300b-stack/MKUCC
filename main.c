#include <pspkernel.h>
#include <pspsdk.h>
#include <pspiofilemgr.h>
#include <string.h>

PSP_MODULE_INFO("MKHybridMod", 0, 1, 0);

void WriteLog(const char *msg) {
    SceUID fd = sceIoOpen("ms0:/PSP/PLUGINS/mkswap/log.txt", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, msg, strlen(msg));
        sceIoClose(fd);
    }
}

void PatchShujinkoToScorpion(void) {
    unsigned int start_addr = 0x08800000;
    unsigned int end_addr   = 0x09600000;
    int patched = 0;

    for (unsigned int addr = start_addr; addr < end_addr; addr += 4) {
        char *ptr = (char*)addr;

        if (memcmp(ptr, "BODY_SHUJINKO", 13) == 0) {
            memcpy(ptr, "BODY_SCORPION", 13);
            patched++;
        }
        else if (memcmp(ptr, "HEAD_SHUJINKO", 13) == 0) {
            memcpy(ptr, "HEAD_SCORPION", 13);
            patched++;
        }
        else if (memcmp(ptr, "body_shujinko_alt.sec", 21) == 0) {
            memcpy(ptr, "body_scorpion_alt.sec", 21);
            patched++;
        }
        else if (memcmp(ptr, "MANTIS", 7) == 0) {
            memcpy(ptr, "HAPKIDO", 8);
            patched++;
        }
        else if (memcmp(ptr, "SHAOLIN FIST", 13) == 0) {
            memcpy(ptr, "MOI FAH\0\0\0\0\0", 13);
            patched++;
        }
        else if (memcmp(ptr, "DAN TIEN DAO", 13) == 0) {
            memcpy(ptr, "MUGAI RYU\0\0\0", 13);
            patched++;
        }
    }

    if (patched > 0) {
        sceKernelDcacheWritebackInvalidateAll();
        WriteLog("INFO: Shujinko patched to Scorpion body & normal attacks successfully!\r\n");
    }
}

int MainThread(SceSize args, void *argp) {
    WriteLog("INFO: MKHybridMod initialized!\r\n");

    for (int i = 0; i < 15; i++) {
        PatchShujinkoToScorpion();
        sceKernelDelayThread(500000);
    }

    return sceKernelExitDeleteThread(0);
}

int main(int argc, char **argv) {
    SceUID thid = sceKernelCreateThread("MKHybridThread", MainThread, 0x18, 0x4000, 0, NULL);
    if (thid >= 0) {
        sceKernelStartThread(thid, 0, NULL);
    }
    return 0;
}

int module_stop(SceSize args, void *argp) {
    return 0;
}
