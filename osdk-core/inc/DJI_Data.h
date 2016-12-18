#ifndef DATASUBSCRIBE_H
#define DATASUBSCRIBE_H

#include <string.h>
#include "DJI_API.h"
#include "DJI_Database.h"
#include "DJI_HardDriver.h"
//! @warning This SDK structure only support SDK V3.2.20 or later

namespace DJI {
namespace onboardSDK {

class DataSubscribe {
 public:
  class DataClause {
   public:
    void* getPtr() const;
    void setPtr(void* value);

    void* ptr;

   public:
    DataClause(void* PTR) : ptr(PTR) {}
  };

 public:
 public:
  DataSubscribe(CoreAPI* API) : api(API) {}

 private:
  void verify();
  void subscribe();
  void add();
  void remove();
  void updateFreq();
  void pause();
  void info();
  void packageInfo();

 private:
  void verifyAuthorty();
  void verifyVersion();

 private:
  CoreAPI* api;
};

class DataPublish {
  //! @todo mirror class with DataSubscribe
 public:
  DataPublish();

 private:
};

class PackageBase {
 public:
 private:
  uint32_t* uidList;
  void* packagePtr;
};

template <uint32_t UID>
class Package : public PackageBase {};

}  // onboardSDK
}  // DJI

#endif  // DATASUBSCRIBE_H
