#ifndef PROJECT21_H

struct offscreen_buffer 
{
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

internal void ApplicationUpdate(offscreen_buffer *Buffer, int BlueOffSet, int GreenOffset);


#define PROJECT21_H
#endif