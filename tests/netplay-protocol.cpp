// SPDX-License-Identifier: GPL-3.0-only
// Small real Winsock host used by test_netplay.py; no ROMs required.
#include "native_netplay.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
int main(int argc,char** argv) {
    if(argc!=4) return 2;
    port::NetSession host;
    std::string error;
    if(!host.Open(argv[1],true,"test-runtime",static_cast<unsigned>(std::atoi(argv[2])),"",error)) {
        std::cerr<<error; return 3;
    }
    const auto limit=std::strtoull(argv[3],nullptr,10);
    for(uint64_t frame=0;frame<=limit;frame++) {
        uint16_t pads[2]{};
        unsigned polls=0;
        for(;;) {
            // Change the sampled input while waiting. Poll must preserve the input
            // it actually sent, rather than replacing it with the latest sample.
            const auto result=host.Poll(frame,static_cast<uint16_t>((frame+polls++)&0xfff),
                                        static_cast<uint32_t>(frame)^0x12345678,pads,frame==limit);
            if(result==port::NetSession::Result::Failed) { std::cerr<<host.Error(); return 5; }
            if(result==port::NetSession::Result::Ready) { std::cout<<frame<<' '<<pads[0]<<' '<<pads[1]<<std::endl; break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}
