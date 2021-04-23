/*
 * id_generator.h
 *
 *  Created on: Mar 9, 2021
 *      Author: ubuntu
 */

#ifndef LIGHT_PSSC_ID_GENERATOR_H_
#define LIGHT_PSSC_ID_GENERATOR_H_

#include <mutex>

template <typename T>
class IDGenerator
{
  std::mutex mtx;
  T id;
public:
  T next()
  {
    std::lock_guard<std::mutex> l(mtx);
    return id++;
  }
};


#endif /* LIGHT_PSSC_ID_GENERATOR_H_ */
