#ifndef SLIDING_AVERAGE_HH_
#define SLIDING_AVERAGE_HH_

/*
 * Sliding Average
 * Author: Thijs Miedema
 * Description: Always up-to-date Sliding Average
 * Date: July 2018
*/

#include <vector>

template
<typename T>
class SlidingAverage {
  public:
    SlidingAverage(unsigned _length) : length(_length), pt(0), values(_length), current_sum(0) {};

    void add_value(T value){
        current_sum -= values[pt];
        current_sum += value;
        values[pt] = value;
        pt = (pt + 1) % length;
    }

    T sum() {
        return current_sum;
    }

    T avg() {
        return current_sum / (T) length;
    }

  private:
    const unsigned length;
    unsigned pt;
    std::vector<T> values;
    T current_sum;
};

template class SlidingAverage<unsigned>;
template class SlidingAverage<int>;
template class SlidingAverage<double>;

#endif /* SLIDING_AVERAGE_HH_ */