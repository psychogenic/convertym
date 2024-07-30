# convertym
Parse and convert a ym file to a simple set of registers to set per sample

This is a slightly hacked up version of the StSound library that converts a YM file, 
e.g. from [here](http://antarctica.no/stuff/atari/YM2/Misc.Games/), for 
the AY8913 chip into a format that pre-calculated
and only includes the registers that actually change in
in a sample, in a concise but non-compressed format.

This allows me to dump them onto an RP2040 and use 
micropython to just read the next set of registers to 
set and then send it to the 
Classic 8-bit era Programmable Sound Generator AY-3-8913
project on tiny tapeout 5:
https://tinytapeout.com/runs/tt05/tt_um_rejunity_ay8913


The file format produced is a header followed by N samples
to be sent out to the chip at SAMPLERATEHz, where each sample
is just the number of registers to set, then a pair of REGISTER,VALUE
bytes for each one

```

        header  =========
        PSYM1 (5 bytes)
        CLOCKFREQ (4 bytes, little end)
        SAMPLERATEHz (1 byte)
        NUMSAMPS (8 bytes, little end)
        /header =========
        Followed by NUMSAMPS entry of form:
         NUMREGSETTINGS (1 byte)
         REGISTER (1 byte) and VALUE (1 byte) (NUMREGSETTINGS times)
```


To build, just compile and link all the files statically, e.g.
  for i in *.cpp; do echo $i; g++ -ggdb -g3 -O3 -c $i; done; g++ -o convertym *.o

For a sample of how I actually use the file, see
[test_rejunity_ay8913](https://github.com/psychogenic/test_rejunity_ay8913)