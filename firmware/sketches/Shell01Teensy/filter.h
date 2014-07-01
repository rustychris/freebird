#ifndef __FILTER_H__
#define __FILTER_H__

//Low pass butterworth filter order=1 alpha1=0.001 
//  from http://www.schwietering.com/jayduino/filtuino/
class Filter
{
public:
  Filter()
    : order(1), inv_alpha1(1000)
  {
    v[0]=0;
    v[1]=0;
  }
private:
  short v[2];
public:
  const uint8_t order;
  const uint32_t inv_alpha1;

  void filter_info(Print &out){
    out.print("filter order: ");
    out.println(order);
    out.print("filter 1/alpha1: ");
    out.println(inv_alpha1);
  }

  short step(short x)
  {
    // Design parameters:
    // butterworth, lowpass, 1st order
    // alpha = 0.001 (100Hz / 100kHz)
    // long type, 16 bits of signal.
    v[0] = v[1];
    v[0] = v[1];
    long tmp = ((((x *  52542L) >>  9)	//= (   3.1317642292e-3 * x)
                 + ((v[0] * 65126L) >> 1)	//+(  0.9937364715*v[0])
                 )+16384) >> 15; // round and downshift fixed point /32768
    
    v[1]= (short)tmp;
    return (short)(((v[0] + v[1]))); // 2^
  }
};

#endif // __FILTER_H__
