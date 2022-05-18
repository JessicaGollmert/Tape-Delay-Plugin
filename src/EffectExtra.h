//
//  EffectExtra.h
//  Additional Plugin Code
//
//  This file is a workspace for developing new DSP objects or functions to use in your plugin.
//

class MyDelay
{
public:
    MyDelay(){
    iBufferSize = 2 * 192000;                   // 2 seconds at 192kHz
    
    pfCircularBuffer = new float[iBufferSize];  // allocate buffer memory
        for (int x = 0; x < iBufferSize; x++) {
            pfCircularBuffer[x] = 0;            // initialise buffer to 0 (silence)
        }
        iBufferWritePos = 0;                    // write to buffer start
        }
    
    ~MyDelay(){
        delete[] pfCircularBuffer;
    }
    
    void write(float input){
          iBufferWritePos ++;
        if (iBufferWritePos == iBufferSize) {
               iBufferWritePos = 0;
        }
            pfCircularBuffer[iBufferWritePos] = input; // write to buffer
      }
    
    float read(int delay){
        int iBufferReadPos = iBufferWritePos - delay;
        if (iBufferReadPos < 0) {
            iBufferReadPos += iBufferSize;
        }
        return pfCircularBuffer[iBufferReadPos]; // read from buffer
    }
    
private:
    float *pfCircularBuffer;
      int iBufferSize, iBufferWritePos;
};

float SoftClipDistortion(float x, float a) {
    float y = x;
    
    if (x > 0) {
        y = a / (a - 1) * (1 - pow(a, -x) );
    }
    else {
        y = a / (a - 1) * (-1 + pow(a, x) );
    }
    
    return y;
}
