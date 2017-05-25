//
// Created by yuwenyong on 17-4-19.
//

#include "tinycore/tinycore.h"


int main(int argc, char **argv) {
    std::string a("abcdefg");
    std::string b("gfedcda");
//    std::string c("IIIdawd123123nn");
    std::string restore;

    GZipCompressor compressor("test.gz");
    compressor.write(a);
    compressor.write(b);
    compressor.close();

    GZipDecompressor decompressor("test.gz");
    decompressor.read(restore);
    std::cout << restore << std::endl;
    restore.clear();

    std::vector<char> buffer;
    GZipCompressor compressor2(buffer);
    compressor2.write(a);
    std::cout << buffer.size() << std::endl;
    compressor2.flush();
    std::cout << buffer.size() << std::endl;
    compressor2.write(b);
    compressor2.close();
    GZipDecompressor decompressor2(buffer);
    decompressor2.read(restore);
    std::cout << restore << std::endl;

    {
//        std::cout << String::toHexStr(c) << std::endl;
        GzipFile f1;
        f1.initWithOutputFile("test.gz");
        f1.write(a);
        f1.write(b);
        f1.close();

        GzipFile f2;
        f2.initWithInputFile("test.gz");
        std::string data = f2.readToString();
        f2.close();
        std::cout << "Uncompress:" << data << std::endl;

        GzipFile f3;
        std::shared_ptr<std::stringstream> ss = std::make_shared<std::stringstream>();
        f3.initWithOutputStream(ss);
        f3.write(a);
        f3.write(b);
        f3.close();

        GzipFile f4;
        f4.initWithInputStream(ss);
        data = f4.readToString();
        f4.close();
        std::cout << "Uncompress:" << data << std::endl;
//        CompressObj o;
//        printf ("first pass\n");
//        ByteArray y = o.compress(a);
//        printf("%d\n", y.size());
//        for (auto c: y) {
//            printf("%x", c);
//        }
//        printf("\n");
//        printf ("second pass\n");
//        std::string z = o.compressToString(a);
//        printf("%d\n", z.size());
//        for (uint8 c: z) {
//            printf("%x", c);
//        }
//        printf("\n");
//        printf ("third pass\n");
//        z = o.flushToString();
//        printf("%d\n", z.size());
//        for (uint8 c: z) {
//            printf("%x", c);
//        }
//        printf("\n");
//        ByteArray z = o.compress(a);
//        for (auto c: z) {
//            printf("%x", c);
//        }
//        printf("\n");
//        std::string x = o.compressToString(a);
//        for (auto c: x) {
//            printf("%x", c);
//        }

//        GzipFile f2;
//        f2.initWithInputFile("test.gz");
//        std::string restore = f2.readToString();
//        std::cout << "Decomp:" << restore << std::endl;
    }

    return 0;
}