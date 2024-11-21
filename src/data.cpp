#include "data.h"

Data& Data::the() {
  static Data data = Data();
  return data;
}
