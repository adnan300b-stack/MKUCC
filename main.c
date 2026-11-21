// ... existing code ...
#define LOG_PATH "ms0:/PSP/PLUGINS/mkswap/log.txt"
#define SETTINGS_PATH_1 "ms0:/PSP/PLUGINS/mkswap/setting.ini"
#define SETTINGS_PATH_2 "ms0:/PSP/PLUGINS/mkswap/settings.ini"
#define BONS_XML_PATH "ms0:/PSP/PLUGINS/mkswap/bons.xml"
#define BASE_MODEL_DIR "ms0:/PSP/model"
// ... existing code ...
const char *header = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<Skeleton>\n";
sceIoWrite(fd, header, strlen(header));
unsigned char *ptr = (unsigned char *)RAM_START;
unsigned char *end = (unsigned char *)RAM_END - 32;
char line[256];
int bone_idx = 0;
while (ptr < end) {
if (memcmp(ptr, "BONE_", 5) == 0 || memcmp(ptr, "Bip01", 5) == 0) {
char name[32];
int j = 0;
while (j < 31 && ptr[j] >= 32 && ptr[j] <= 126) {
name[j] = ptr[j];
j++;
}
name[j] = '\0';
if (j > 4) {
unsigned int hex_id = *(unsigned int *)(ptr - 4);
if (hex_id == 0) hex_id = 0x1000 + bone_idx;
if (bone_idx == 0) {
snprintf(line, sizeof(line), "  <Bone id="%d" name="%s" hex="0x%08X" parent="0x00000000">\n",
bone_idx, name, hex_id);
} else {
snprintf(line, sizeof(line), "    <Bone id="%d" name="%s" hex="0x%08X" parent="0x%08X" />\n",
bone_idx, name, hex_id, 0x1000 + bone_idx - 1);
}
sceIoWrite(fd, line, strlen(line));
bone_idx++;
ptr += j;
continue;
}
}
ptr++;
}
if (bone_idx > 0) {
const char *close_root = "  </Bone>\n";
sceIoWrite(fd, close_root, strlen(close_root));
}
const char *footer = "</Skeleton>\n";
sceIoWrite(fd, footer, strlen(footer));
sceIoClose(fd);
write_log("INFO: Generated bons.xml tree successfully.");
}
// ... existing code ...
void bind_custom_model(const char *chr, const char *rel_path) {
ensure_dir("ms0:/PSP");
ensure_dir(BASE_MODEL_DIR);
char dir_path[128];
snprintf(dir_path, sizeof(dir_path), "%s/%s", BASE_MODEL_DIR, chr);
ensure_dir(dir_path);
char bon_ini_path[160];
snprintf(bon_ini_path, sizeof(bon_ini_path), "%s/bon.ini", dir_path);
BoneMap maps[64];
int map_count = parse_bon_ini(bon_ini_path, maps, 64);
const char *exts[] = {"glb", "gltf", "obj", "fbx", "dae", "smd", "sec", "gmo"};
char model_file[192];
int found_model = 0;
const char *clean_path = rel_path;
if (strncmp(clean_path, "@model/", 7) == 0) {
clean_path += 7;
}
snprintf(model_file, sizeof(model_file), "%s/%s", BASE_MODEL_DIR, clean_path);
SceUID mfd = sceIoOpen(model_file, PSP_O_RDONLY, 0);
if (mfd < 0) {
snprintf(model_file, sizeof(model_file), "%s/%s", dir_path, clean_path);
mfd = sceIoOpen(model_file, PSP_O_RDONLY, 0);
}
if (mfd < 0) {
for (int i = 0; i < 8; i++) {
snprintf(model_file, sizeof(model_file), "%s/%s.%s", dir_path, chr, exts[i]);
mfd = sceIoOpen(model_file, PSP_O_RDONLY, 0);
if (mfd >= 0) break;
}
}
if (mfd >= 0) {
SceIoStat stat;
sceIoGetstat(model_file, &stat);
if (stat.st_size > 0) {
void *model_buf = malloc(stat.st_size);
if (model_buf) {
sceIoRead(mfd, model_buf, stat.st_size);
for (int b = 0; b < map_count; b++) {
unsigned char *mptr = (unsigned char *)model_buf;
size_t blen = strlen(maps[b].model_bone);
for (SceOff offset = 0; offset <= stat.st_size - (SceOff)blen; offset++) {
if (memcmp(mptr + offset, maps[b].model_bone, blen) == 0) {
*(unsigned int *)(mptr + offset) = maps[b].game_bone_hex;
}
}
}
free(model_buf);
}
}
sceIoClose(mfd);
found_model = 1;
}
char log_msg[256];
if (found_model) {
snprintf(log_msg, sizeof(log_msg), "INFO: Bound model %s for %s with %d bones.", model_file, chr, map_count);
} else {
snprintf(log_msg, sizeof(log_msg), "WARN: Folder %s created. Place model and bon.ini inside.", dir_path);
}
write_log(log_msg);
}
void process_settings() {
SceUID fd = sceIoOpen(SETTINGS_PATH_1, PSP_O_RDONLY, 0);
if (fd < 0) {
fd = sceIoOpen(SETTINGS_PATH_2, PSP_O_RDONLY, 0);
}
if (fd < 0) {
write_log("ERROR: Could not open setting.ini or settings.ini");
return;
}
char buf[2048];
int bytes = sceIoRead(fd, buf, sizeof(buf) - 1);
sceIoClose(fd);
if (bytes <= 0) return;
buf[bytes] = '\0';
int do_getbon = 0;
char swap_c1[32] = {0}, swap_c2[32] = {0};
char cbm_chr[32] = {0}, cbm_path[96] = {0};
char *line = strtok(buf, "\r\n");
while (line) {
while (*line == ' ' || *line == '\t') line++;
if (strncmp(line, "getbon=true", 11) == 0) {
do_getbon = 1;
} else if (strncmp(line, "chang(", 6) == 0) {
char *arrow = strstr(line + 6, "->");
int arrow_len = 2;
if (!arrow) {
arrow = strchr(line + 6, '>');
arrow_len = 1;
}
char *close_p = strchr(line + 6, ')');
if (arrow && close_p && arrow < close_p) {
int len1 = arrow - (line + 6);
int len2 = close_p - (arrow + arrow_len);
if (len1 > 0 && len1 < 31 && len2 > 0 && len2 < 31) {
strncpy(swap_c1, line + 6, len1);
swap_c1[len1] = '\0';
strncpy(swap_c2, arrow + arrow_len, len2);
swap_c2[len2] = '\0';
}
}
} else if (strncmp(line, "cbm(", 4) == 0) {
char *comma = strchr(line + 4, ',');
char *close_p = strchr(line + 4, ')');
if (comma && close_p && comma < close_p) {
int len1 = comma - (line + 4);
int len2 = close_p - (comma + 1);
if (len1 > 0 && len1 < 31 && len2 > 0 && len2 < 95) {
strncpy(cbm_chr, line + 4, len1);
cbm_chr[len1] = '\0';
strncpy(cbm_path, comma + 1, len2);
cbm_path[len2] = '\0';
}
}
}
line = strtok(NULL, "\r\n");
}
// ... existing code ...
