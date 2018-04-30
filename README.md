# srt
A Srt Subtitles CLI Tool to Tune Timestamps

## Installation
0. Prerequisites:
  - g++
  - boost library
1. Download this project and another project [toolbox](https://github.com/dayanuyim/toolbox.git) in the same folder.
2. Enter 'srt' subfolder and make. 

## Usage

  srt [options] \<command\> \<arguments\>

### OPTIONS
  **-c** condense multile lines to one line. (This is useful if you want to merge multi-lang later)
    
  **-f=[uwm]** the newline of output: unix, windows, or mac

### COMMANDS
  **merge**  Merge multiple N srt files. N >= 1
  
      merge <file 1>...<file N>.

  **offset**  Offset a relative timestamp.
  
    offset +/-<time> <file>
      <time> = 'HH:mm:ss,fff'
      
    offset -<n> <time> <file>
      <n> is srt SN. The form is like above, but using specified timestamp.

  **sync**  Tune the offset and the frequency to fit the specified timestamps.
  
    sync -<n1> <time1> -<n2> <time2> <file>.
      <n1>, <n2> is srt SN. <time1> and <time2> are the specified timestamps.
      
    sync -<n1> +/-<time1> -<n2> +/-<time2> <file>.
      <n1>, <n2> is srt SN. if <time1> or <time2> is prefixed with '+' or '-', it is a offset to the SN.
