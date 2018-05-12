/*******************************************************************************
 *编译方法: gcc -std=c99 -O2 -s -o RgssadUnpacker RgssadUnpacker.c
 ******************************************************************************/

#ifndef __RU_STRING__
#define __RU_STRING__

static char RUS_INFO[] =
"===============================================================================\n"
"RgssadUpacker - 命令行的Rgssad和Rgss2a文件解包器 - Version %s\n"
"更多信息请见 http://blog.csdn.net/rgss2ad/archive/2011/02/16/6187475.aspx\n"
"===============================================================================\n";

static char RUS_HELP[] =
"用法:\n"
"    RgssadUnpacker <Rgssad文件名> [<可选参数>]\n"
"\n"
"可选参数(大小写不敏感):\n"
"    -pw=<密码>,          --Password=<密码>\n"
"        强制使用<密码>当作密码来解包, *不推荐使用*, 16进制使用 '0x' 前缀\n"
"    -od=<输出目录>,      --OutDir=<输出目录>\n"
"        将解包后文件输出至目录<输出目录>而不是目录<Rgssad文件名>_Unpacked\n"
"    -lf=[<记录文件名>],  --LogFile=[<记录文件名>]\n"
"        生成解包记录文件<记录文件名>或<Rgssad文件名>_Unpacked.log\n"
"    -rn,                 --Rename\n"
"        使用文件序号代替解包后文件原名, 仅当输出文件名异常时才应使用这个参数\n"
"\n"
"例子:\n"
"    RgssadUnpacker Game.rgssad\n"
"    RgssadUnpacker Game.rgss2a\n"
"    RgssadUnpacker Game.rgssad -lf -rn\n"
"    RgssadUnpacker Game.rgssad -pw=0xDEADCAFE -od=C:\\Temp\\ -lf=Game.log\n";

static char RUS_E_FILE_OPEN[] =
    "无法打开文件 \"%s\"\n";
static char RUS_E_RGSSAD_SIGN[] =
    "无法识别的rgssad标识, 请确定这是一个rgssad文件或rgss2a文件\n";
static char RUS_E_ARGUMENT[] =
    "未知参数 \"%s\"\n";
static char RUS_E_DIR_LOCATE[] =
    "无法定位到目录 \"%s\"\n";
static char RUS_E_CALC_PASSWORD[] =
    "无法计算出密码, 可能是文件损坏或采取了不支持的加密方法\n";
static char RUS_E_UNPACK[] =
    "解包失败, 未知错误 0x%08X\n";

static char RUS_WAIT_UNPACK[] =
    "正在解包, 请稍候....\n";
static char RUS_WAIT_CACL_PASSWORD[] =
    "正在计算密码, 请稍候....\n";
static char RUS_SUCCESS[] =
    "没有检测到错误, 解包很可能成功了\n";

static const char *RUS_LOG_HEADER =
    "文件序号\x9""文件长度\x9""文件名\x9状态\n";
static const char *RUS_LOG_PASSWORD =
    "密码: 0x%08X\n";
static const char *RUS_LOG_FILE_SUCCESS =
    "成功\n";
static const char *RUS_LOG_FILE_FAIL =
    "失败\n";

#endif //__RU_STRING__

static const char *VERSION = "0.1";

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>

static const int MAX_PATH = 260;
static const int RGSSAD_SIGN_LENGTH = 8;
static const char *RGSSAD_SIGN = "RGSSAD\x0\x1";
static const int BUF_SIZE = 0x10000;

//错误
enum ruErrors
{
    E_NONE = 0,         //没有错误, 一切正常
    E_RGSSAD_SIGN,      //rgssasd文件标识 不符合
    E_CALC_PASSWORD,    //无法得到正确的密码
    E_FILE_TOO_LONG,    //文件过大(超过 2GB)
    E_FILE_OPEN,        //打开(创建)文件失败
    E_FILE_READ,        //读取文件失败(多数情况下为 读取长度 超出 被读取文件长度)
    E_FILENAME_TOO_LONG,//文件名过长(超过 MAX_PATH - 1)
    E_ARGUMENT,         //存在未知参数
    E_DIR_LOCATE,       //无法定位到输出目录
    E_UNPACK            //解包失败
};

///使用 MultiByteToWideChar() 和 WideCharToMultiByte()
#include <windows.h>

/*******************************************************************************
 * 字符串编码转换 UTF-8 -> ANSI
 * str: 需转换的字符串
 * 返回值: str
 ******************************************************************************/
static inline char *ruUtf8ToAnsi(char *str)
{
    int l = strlen(str) + 1;
    wchar_t *wstr = malloc(l * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr, l * 2);
    WideCharToMultiByte(CP_ACP, 0, wstr, -1, str, l, "_", NULL);
    free(wstr);
    return str;
}


/*******************************************************************************
 * 直接创建目录
 * dirs: 目标目录字符串
 * pos: 从dirs中的pos位置开始创建目录, pos位置之前的目录必须存在
 * 返回值: 成功时返回 0, 失败时返回 非0
 ******************************************************************************/
static inline int ruCreateDirs(const char *dirs, int pos)
{
    int result = 0;
    char buf[MAX_PATH];
    int l = strlen(dirs);
    if (l >= MAX_PATH) return -1;
    memcpy(buf, dirs, l + 1);
    for (int i = pos; i < l; ++i)
        if (buf[i] == '\\')
        {
            buf[i] = 0;
            result |= mkdir(buf);
            buf[i] = '\\';
        }
    return result;
}

/*******************************************************************************
 * 检查并创建目录
 * dirs: 目标目录字符串
 * 返回值: 成功时返回 0, 失败时返回 非0
 ******************************************************************************/
static inline int ruCheckDirs(const char *dirs)
{
    char buf[MAX_PATH];
    int l = strlen(dirs);
    if (l >= MAX_PATH) return -1;
    memcpy(buf, dirs, l + 1);
    for (int i = l - 1; i >= 0; --i)
        if (buf[i] == '\\')
        {
            buf[i] = 0;
            if (access(buf, 0) == 0)
            {
                buf[i] = '\\';
                return ruCreateDirs(buf, i + 1);
            }
            buf[i] = '\\';
        }
    return ruCreateDirs(buf, 0);
}

/*******************************************************************************
 * 检查 rgssad文件标识 是否符合
 * rgssadFile: 以 "rb" 方式打开的 rgssad文件
 * 返回值: rgssad文件标识 符合时返回 E_NONE, 不符合或失败时返回 其它ruErrors
 ******************************************************************************/
int ruCheckRgssadSign(FILE* rgssadFile)
{
    FILE *rf = rgssadFile;
    int pos_bak = ftell(rf);
    rewind(rf);

    char rgssadSign[RGSSAD_SIGN_LENGTH];
    fread(rgssadSign, RGSSAD_SIGN_LENGTH, 1, rf);

    fseek(rf, pos_bak, SEEK_SET);

    if (strncmp(rgssadSign, RGSSAD_SIGN, RGSSAD_SIGN_LENGTH) != 0)
        return E_RGSSAD_SIGN; else return E_NONE;
}

/*******************************************************************************
 * 检查 密码 是否正确
 * rgssadFile: 以 "rb" 方式打开的 rgssad文件
 * password: 密码
 * 返回值: 密码 正确时返回 E_NONE, 不正确或失败时返回 其它ruErrors
 ******************************************************************************/
int ruCheckPassword(FILE *rgssadFile, int password)
{
    FILE *rf = rgssadFile;
    rewind(rf);

    //获取 rgssad文件长度
    fseek(rf, 0, SEEK_END);
    int rgssadFileLength = ftell(rf);
    fseek(rf, RGSSAD_SIGN_LENGTH, SEEK_SET);

    //判断 rgssad文件长度 是否有效
    if (rgssadFileLength < 0) return E_FILE_TOO_LONG;

    while (ftell(rf) < rgssadFileLength)
    {
        //判断 文件名长度在rgssad文件中的位置 是否有效
        if ((ftell(rf) + 4 < 0) || (ftell(rf) + 4 > rgssadFileLength))
            return E_FILE_READ;

        //获取 文件名长度
        int fileNameLength;
        fread(&fileNameLength, 4, 1, rf);
        fileNameLength ^= password;

        //判断 文件名长度 是否有效
        if ((fileNameLength < 0) || (fileNameLength > MAX_PATH - 1))
            return E_FILENAME_TOO_LONG;

        //判断 文件名在rgssad文件中的位置 是否有效
        if ((ftell(rf) + fileNameLength < 0) ||
            (ftell(rf) + fileNameLength > rgssadFileLength))
            return E_FILE_READ;

        //跳过 文件名
        fseek(rf, fileNameLength, SEEK_CUR);

        //判断 文件长度在rgssad文件中的位置 是否有效
        if ((ftell(rf) + 4 < 0) || (ftell(rf) + 4 > rgssadFileLength))
            return E_FILE_READ;

        //获取 文件长度
        int fileLength;
        fread(&fileLength, 4, 1, rf);
        for (int i = -1; i < fileNameLength; ++i) password = password * 7 + 3;
        fileLength ^= password;
        password = password * 7 + 3;

        //判断 文件在rgssad文件中的位置 是否有效
        if ((fileLength < 0) ||
            (ftell(rf) + fileLength < 0) ||
            (ftell(rf) + fileLength > rgssadFileLength))
            return E_FILE_READ;

        //跳过 文件
        fseek(rf, fileLength, SEEK_CUR);
    }
    return E_NONE;
}

/*******************************************************************************
 * 计算 密码
 * rgssadFile: 以 "rb" 方式打开的 rgssad文件
 * password: 计算出的 密码
 * 返回值: 得到 密码 时返回 E_NONE, 未得到或失败时返回 其它ruErrors
 ******************************************************************************/
int ruCalcPassword(FILE *rgssadFile, int *password)
{
    FILE *rf = rgssadFile;
    rewind(rf);

    //跳过 rgssad文件标识
    fseek(rf, RGSSAD_SIGN_LENGTH, SEEK_SET);

    int fileNameLength;
    fread(&fileNameLength, 4, 1, rf);
    for (int i = 0; i < MAX_PATH; ++i)
        if (ruCheckPassword(rf, fileNameLength ^ i) == 0)
        {
            *password = fileNameLength ^ i;
            return E_NONE;
        }

    return E_CALC_PASSWORD;
}

/*******************************************************************************
 * 直接以password解包文件 (ruUnpack省略对多种错误的检测, 请先用ruCheckPassword检测)
 * rgssadFile: 以 "rb" 方式打开的 rgssad文件
 * password: 密码
 * isRenameFile: 是否更改文件名为文件序号(避免因为文件名非法无法解包出文件)
 * lofFile: 以 "w" 方式打开的 记录文件, 值为 NULL 时不创建 记录文件
 * 返回值: 解包成功时返回 E_NONE, 失败时返回 其它ruErrors
 ******************************************************************************/
int ruUnpack(FILE *rgssadFile, int password, bool isRenameFile, FILE *logFile)
{
    FILE *rf = rgssadFile;
    rewind(rf);

    int result = E_NONE;

    //文件序号
    int fileNum = 0;

    //获取 rgssad文件长度
    fseek(rf, 0, SEEK_END);
    int rgssadFileLength = ftell(rf);
    fseek(rf, RGSSAD_SIGN_LENGTH, SEEK_SET);

    //输出 logFile 表头
    if (logFile != NULL)
    {
        fprintf(logFile, RUS_LOG_PASSWORD, password);
        fprintf(logFile, RUS_LOG_HEADER);
    }

    while (ftell(rf) < rgssadFileLength)
    {
        ++fileNum;

        //获取 文件名长度
        int fileNameLength;
        fread(&fileNameLength, 4, 1, rf);
        fileNameLength ^= password;
        password = password * 7 + 3;

        //获取 文件名
        char fileName[MAX_PATH];
        fread(fileName, fileNameLength, 1, rf);
        for (int i = 0; i < fileNameLength; ++i)
        {
            fileName[i] ^= password;
            password = password * 7 + 3;
        }
        fileName[fileNameLength] = 0;

        //获取 文件长度
        int fileLength;
        fread(&fileLength, 4, 1, rf);
        fileLength ^= password;
        password = password * 7 + 3;

        //输出 logFile 内容
        if (logFile != NULL)
            fprintf(logFile, "%08d\x9% 8d\x9%s\x9", fileNum, fileLength,
                fileName);

        //生成 文件名
        if (isRenameFile) sprintf(fileName, "%08X", fileNum);

        ////转换 文件名编码 (UTF-8 -> ANSI)
        ruUtf8ToAnsi(fileName);

        //创建 目录
        if (ruCheckDirs(fileName) != 0) result = E_FILE_OPEN;

        //备份 password
        int passwordBak = password;

        //输出 文件
        FILE *uf = fopen(fileName, "wb");
        if (uf != NULL)
        {
            int buf[BUF_SIZE >> 2];
            while (fileLength > 0)
            {
                int length = fileLength < BUF_SIZE ? fileLength : BUF_SIZE;
                fread(buf, length, 1, rf);
                for (int i = 0; i < ((length + 3) >> 2); ++i)
                {
                    buf[i] ^= password;
                    password = password * 7 + 3;
                }
                fwrite(buf, length, 1, uf);
                fileLength -= length;
            }
            fclose(uf);
            if (logFile != NULL) fprintf(logFile, RUS_LOG_FILE_SUCCESS);
        }
        else
        {
            result = E_FILE_OPEN;
            if (logFile != NULL) fprintf(logFile, RUS_LOG_FILE_FAIL);
        }

        //恢复 password
        password = passwordBak;

    }
    return result;
}

/*******************************************************************************
 * 主函数 >_<
 * 返回值: 成功时返回 0, 失败时返回 非0
 ******************************************************************************/
int main(int argc, char **argv)
{
    printf(ruUtf8ToAnsi(RUS_INFO), VERSION);

    //无参数, 显示帮助
    if (argc < 2)
    {
        printf(ruUtf8ToAnsi(RUS_HELP));
        return 0;
    }

    FILE *rgssadFile = fopen(argv[1], "rb");
    if (rgssadFile == NULL)
    {
        printf(ruUtf8ToAnsi(RUS_E_FILE_OPEN), argv[1]);
        return E_FILE_OPEN;
    }

    //检查 rgssad文件标识
    if (ruCheckRgssadSign(rgssadFile) != E_NONE)
    {
        printf(ruUtf8ToAnsi(RUS_E_RGSSAD_SIGN));
        fclose(rgssadFile);
        return E_RGSSAD_SIGN;
    }

    int password = 0;
    bool isForcePassword = false;
    bool isLogFile = false;
    bool isRenameFile = false;

    char outDir[MAX_PATH];
    //默认输出目录
    sprintf(outDir, "%s_Unpacked\\", argv[1]);

    char logFileName[MAX_PATH];
    //默认记录文件名
    sprintf(logFileName, "%s_Unpacked.log", argv[1]);

    //解析 可选参数
    for (int i = 2; i < argc; ++i)
    {
        if ((argv[i][0] == '-') && (argv[i][1] == '-'))
        {
            char tmp[8];
            memcpy(tmp, &argv[i][2], 8);
            for (int i = 0; i < 8; ++i) tmp[i] = tolower(tmp[i]);
            if (strncmp(tmp, "password", 8) == 0)
            {
                if ((argv[i][11] == '0') || (argv[i][12] == 'x'))
                    sscanf(&argv[i][11], "%x", &password);
                else
                    sscanf(&argv[i][11], "%d", &password);
                isForcePassword = true;
                continue;
            }
            if (strncmp(tmp, "outdir", 6) == 0)
            {
                sscanf(&argv[i][9], "%s", outDir);
                continue;
            }
            if (strncmp(tmp, "logfile", 7) == 0)
            {
                isLogFile = true;
                if (strlen(argv[i]) > 9)
                    sscanf(&argv[i][10], "%s", logFileName);
                continue;
            }
            if (strncmp(tmp, "rename", 6) == 0)
            {
                isRenameFile = true;
                continue;
            }
        }
        if (argv[i][0] == '-')
        {

            char tmp[2];
            memcpy(tmp, &argv[i][1], 2);
            for (int i = 0; i < 2; ++i) tmp[i] = tolower(tmp[i]);
            if (strncmp(tmp, "pw", 2) == 0)
            {
                if ((argv[i][4] == '0') || (argv[i][5] == 'x'))
                    sscanf(&argv[i][4], "%x", &password);
                else
                    sscanf(&argv[i][4], "%d", &password);
                isForcePassword = true;
                continue;
            }
            if (strncmp(tmp, "od", 2) == 0)
            {
                sscanf(&argv[i][4], "%s", outDir);
                continue;
            }
            if (strncmp(tmp, "lf", 2) == 0)
            {
                isLogFile = true;
                if (strlen(argv[i]) > 3)
                    sscanf(&argv[i][4], "%s", logFileName);
                continue;
            }
            if (strncmp(tmp, "rn", 2) == 0)
            {
                isRenameFile = true;
                continue;
            }
        }
        printf(ruUtf8ToAnsi(RUS_E_ARGUMENT), argv[i]);
        fclose(rgssadFile);
        return E_ARGUMENT;
    }


    char oldDir[MAX_PATH];
    getcwd(oldDir, MAX_PATH);

    //定位到输出目录
    if (outDir[strlen(outDir) - 1] != '\\')
    {
        outDir[strlen(outDir) + 1] = 0;
        outDir[strlen(outDir)] = '\\';
    }
    ruCheckDirs(outDir);
    if (chdir(outDir) != 0)
    {
        printf(ruUtf8ToAnsi(RUS_E_DIR_LOCATE), outDir);
        fclose(rgssadFile);
        return E_DIR_LOCATE;
    }

    //计算密码
    if (!isForcePassword)
    {
        printf(ruUtf8ToAnsi(RUS_WAIT_CACL_PASSWORD));
        if (ruCalcPassword(rgssadFile, &password) != E_NONE)
        {
            printf(ruUtf8ToAnsi(RUS_E_CALC_PASSWORD));
            return E_CALC_PASSWORD;
        };
    }

    FILE *logFile = NULL;
    if (isLogFile)
    {
        chdir(oldDir);
        logFile = fopen(logFileName, "w");
        chdir(outDir);
    }

    //解包
    printf(ruUtf8ToAnsi(RUS_WAIT_UNPACK));
    int result = ruUnpack(rgssadFile, password, isRenameFile, logFile);

    fclose(rgssadFile);
    if (logFile != NULL) fclose(logFile);
    chdir(oldDir);

	if (result == E_NONE)
    {
        printf(ruUtf8ToAnsi(RUS_SUCCESS));
        return E_NONE;
    }
    else
    {
        printf(ruUtf8ToAnsi(RUS_E_UNPACK));
        return E_UNPACK;
    }
}
