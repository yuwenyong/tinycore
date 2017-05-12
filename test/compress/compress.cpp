//
// Created by yuwenyong on 17-4-19.
//

#include "tinycore/compress/gzip.h"


int main(int argc, char **argv) {
    std::string a("abcdefg");
    std::string b("gfedcda");
    std::string restore;
//
//    GZipCompressor compressor("test.gz");
//    compressor.write(a);
//    compressor.write(b);
//    compressor.close();

//    GZipDecompressor decompressor("test.gz");
//    decompressor.read(restore);
//    std::cout << restore << std::endl;
//    restore.clear();
//
//    std::vector<char> buffer;
//    GZipCompressor compressor2(buffer);
//    compressor2.write(a);
//    std::cout << buffer.size() << std::endl;
//    compressor2.flush();
//    std::cout << buffer.size() << std::endl;
//    compressor2.write(b);
//    compressor2.close();
//    GZipDecompressor decompressor2(buffer);
//    decompressor2.read(restore);
//    std::cout << restore << std::endl;

    {
        GzipFile f;
        f.initWithOutputFile("testx.gz");
        f.write(a);
        f.write(b);
        f.close();

//        GzipFile f2;
//        f2.initWithInputFile("test.gz");
//        std::string restore = f2.readToString();
//        std::cout << "Decomp:" << restore << std::endl;
    }

    return 0;
}