/*
 * convertym
 * Copyright (C) 2024 Pat Deegan, https://psychogenic.com
 * 
 * Usage:
 * ./convertym [-p] infile.ym outfile.psym
 * 
 * converts a YM file, e.g.from
 * http://antarctica.no/stuff/atari/YM2/Misc.Games/
 * for the AY8913 chip into a format that pre-calculated
 * and only includes the registers that actually change in
 * in a sample, in a concise but non-compressed format.
 * 
 * This allows me to dump them onto an RP2040 and use 
 * micropython to just read the next set of registers to 
 * set and then send it to the 
 * Classic 8-bit era Programmable Sound Generator AY-3-8913
 * project on tiny tapeout 5:
 * https://tinytapeout.com/runs/tt05/tt_um_rejunity_ay8913
 * 
 * Two file formats are available by default: 
 *   - the "psym" described below
 *   - pure python (using the -p flag)
 * 
 * PSYM:
 * The file format produced is a header followed by N samples
 * to be sent out to the chip at SAMPLERATEHz, where each sample
 * is just the number of registers to set, then a pair of REGISTER,VALUE
 * bytes for each one
 * 
         * header  =========
         * PSYM1 (5 bytes)
         * CLOCKFREQ (4 bytes, little end)
         * SAMPLERATEHz (1 byte)
         * NUMSAMPS (8 bytes, little end)
         * /header =========
         * Followed by NUMSAMPS entry of form:
         *  NUMREGSETTINGS (1 byte)
         *  REGISTER (1 byte) and VALUE (1 byte) (NUMREGSETTINGS times)
 * 
 * 
 * Pure Python:
 *      Will output a file with a Song = []
 *      Each entry in the list is a sample.
 *      Each entry in the sample is a register setting, (REG, VAL)
 *       SongInfo = {'clock': 2000000, 'rate': 2, 'num': 3646}
 *       Song = [
 *               [(0,0),(1,0),(2,0),(3,0),(4,0),(5,0),(6,0),(7,255),(8,0),(9,0),(10,0),(11,0),(12,0),(13,0),(14,0),(15,0)]
 *               [(4,255),(7,251),(10,8),(13,15)]
 *               [(0,214),(7,250),(8,9)],
 *               # ...
 *       ]
 *
 * 
 * To build it, you need the StSound StSoundLibrary from 
 * https://github.com/arnaud-carre/StSound/tree/main/StSoundLibrary
 * 
 * I just stick this file in there, then build and link all the files statically, e.g.
 *   for i in *.cpp; do echo $i; g++ -ggdb -g3 -O3 -c $i; done; g++ -o convertym *.o
 * 
 * For a sample of how I actually use the file, see
 * https://github.com/psychogenic/test_rejunity_ay8913
 */

#include "StSoundLibrary.h"
#include "YmMusic.h"
#include <iostream>
#include <vector>
#include <fstream>
#define SKIP_DUPS
#define CLOCK_FREQ_HZ 2000000
#define SAMPLE_RATE_HZ 50

typedef struct {
    uint8_t reg;
    uint8_t val;
} RegisterValue;

typedef struct {
    uint8_t num;
    RegisterValue values[YMNUMREGISTERS];
    
} RegisterSettings;


int main(int argc, char* argv[]) {
        const char * infile;
        const char * outfile;
        bool purePython = false;
        uint32_t clockFreq = CLOCK_FREQ_HZ;
        
        uint8_t rateHz = SAMPLE_RATE_HZ;
        
        ymsample buf[10];
        std::vector<RegisterSettings*> samples;
        bool skip_duplicates = false;
        #ifdef SKIP_DUPS
        skip_duplicates = true;
        #endif
        if (argc < 3) {
            std::cerr << "Usage: ymconvert [-p] FILE.ym OUTFILE" << std::endl;
            return -1;
        }
        
        if (argc > 3) {
                if (std::string(argv[1]) == "-p") {
                        std::cout << "Pure python" << std::endl;
                        purePython = true;
                }
                infile = argv[2];
                outfile = argv[3];
        } else {
                
                infile = argv[1];
                outfile = argv[2];
        }
        
        YMMUSIC * song = ymMusicCreate();
        CYmMusic * music = (CYmMusic*)song;
        if (! ymMusicLoad(song, infile)) {
            std::cerr << "Can't find " << infile << std::endl;
            return -2;
        }
        
        ymMusicInfo_t info;
        ymMusicGetInfo(song, &info);
        std::cout << "Name: " << info.pSongName << std::endl;
        std::cout << "Author: " << info.pSongAuthor << std::endl;
        std::cout << "Comment: " << info.pSongComment << std::endl;
        std::cout << "Duration: " << info.musicTimeInSec/60 << ":" << info.musicTimeInSec%60 << std::endl;
        std::cout << "Driver: " << info.pSongPlayer << std::endl;
        std::cout << music;
        ymMusicPlay(song);
        int count = 0;
        int chip_register_value[YMNUMREGISTERS];
        for (uint8_t i=0; i<YMNUMREGISTERS; i++) {
                chip_register_value[i] = -1;
        }
        while (! ymMusicIsOver(song)) {
            ymMusicCompute(song,buf,1);
            
            if (YMCurrentSample.ready) {
                
                RegisterSettings * settings = new RegisterSettings; // not even deleting, don't care
                std::cout << "Sample " << count << std::endl;
                uint8_t num_set = 0;
                uint8_t num_new = 0;
                for (int i=0; i<YMNUMREGISTERS; i++) {
                        if (YMCurrentSample.registers[i] >=0 ) {
                                // it is set
                                if (YMCurrentSample.registers[i] != chip_register_value[i]) {
                                        // it has changed
                                        num_new++;
                                }
                        }
                }
                
                for (int i=0; i<YMNUMREGISTERS; i++) {
                    if (YMCurrentSample.registers[i] >=0 ) {
                        
                        // want to set this if:
                        // skip_dups AND is new
                        //  or
                        // skip_dups and NO new and none set yet
                        //  or
                        // not skip dups
                        
                        if ( (!skip_duplicates) || 
                                (skip_duplicates && (!num_new) && (!num_set))
                                ||
                                (skip_duplicates && YMCurrentSample.registers[i] != chip_register_value[i])
                        ) {
                                settings->values[num_set].reg = i;
                                settings->values[num_set].val = YMCurrentSample.registers[i];
                                num_set++;
                                
                                chip_register_value[i] = YMCurrentSample.registers[i];
                                std::cout << "\t" << i << "," << YMCurrentSample.registers[i] << std::endl;
                        }
                    } 
                }
                
                if (num_set) {
                    settings->num = num_set;
                    samples.push_back(settings);
                    count++;
                } else {
                    delete settings;
                }
                ymResetCurrentSample();
            }
        }
        /*
         * 
         * FORMAT
         * 
         * header  =========
         * PSYM1 (5 bytes)
         * CLOCKFREQ (4 bytes, little end)
         * SAMPLERATEHz (1 byte)
         * NUMSAMPS (8 bytes, little end)
         * /header =========
         * Followed by NUMSAMPS entry of form:
         *  NUMREGSETTINGS (1 byte)
         *  REGISTER (1 byte) and VALUE (1 byte) (NUMREGSETTINGS times)
         * 
         */
        std::cout << "collected " << samples.size() << " samples, writing to " << outfile << std::endl;
        std::size_t numsamps = samples.size();
        if (purePython) {
                
                std::ofstream fs(outfile, std::ios::out | std::ios::binary);
                fs << "SongInfo = {'clock': " << clockFreq << ", 'rate': " << rateHz << ", 'num': " << numsamps << "}\n";
                
                fs << "Song = [\n";
                
                uint8_t reg_count = 0;
                for (RegisterSettings * s : samples) {
                        // std::cout << (int)s->num << std::endl;
                        if (! reg_count) {
                                fs << "\t";
                        }
                        fs << "[";
                        for (uint8_t j=0; j<s->num; j++) {
                                if (j) {
                                        fs << ",";
                                }
                                fs << "(" << (int)s->values[j].reg << "," <<(int) s->values[j].val << ")";
                                reg_count ++;
                        }
                        fs << "],";
                        if (reg_count > 10) {
                                reg_count = 0;
                                fs << "\n";
                        }
                }
                fs << "]\n";
                
        } else {
                std::ofstream fs(outfile, std::ios::out | std::ios::binary);
                fs << "PSYM1";
                fs.write((char*)&clockFreq, sizeof(uint32_t));
                fs << rateHz;
                fs.write((char*)&(numsamps), sizeof(std::size_t));
                
                for (RegisterSettings * s : samples) {
                        // std::cout << (int)s->num << std::endl;
                        fs << s->num;
                        for (uint8_t j=0; j<s->num; j++) {
                                fs << s->values[j].reg;
                                fs << s->values[j].val;
                        }
                }
                fs.close();
        }
        
        

        
        return 0;
        
}