#pragma once
#include <string>
#include <stdexcept>
class CADSComm {
public:
    template<class... A> bool ADSRead(A...) { throw std::runtime_error("direct ADS forbidden"); }
    template<class... A> bool ADSWrite(A...) { throw std::runtime_error("direct ADS forbidden"); }
    template<class... A> bool ADSReadSum(A...) { throw std::runtime_error("direct ADS forbidden"); }
    template<class... A> bool ADSWriteSum(A...) { throw std::runtime_error("direct ADS forbidden"); }
    std::string GetLastErrorCopy() const { return "memory PLC"; }
};
