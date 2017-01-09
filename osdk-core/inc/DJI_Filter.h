#ifndef FILTER_H
#define FILTER_H

#include "DJI_API.h"

namespace DJI {
namespace onboardSDK {

class Filter {
 public:
  typedef uint8_t Order;

 public:
  Filter(CoreAPI* API = 0);

  Order registerFilter(uint8_t cmdSet, uint8_t cmdID);

  CoreAPI* getAPI() const;
  void setAPI(CoreAPI* value);

 protected:
  CoreAPI* api;
};

}  // onboardSDK
}  // DJI
#endif  // FILTER_H