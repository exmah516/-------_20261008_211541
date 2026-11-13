#pragma once
#include "ads_communication.h"
#include <initializer_list>
void baseline_capture(const AdsOutputCommand&,std::initializer_list<double>);
void baseline_endpoint(std::initializer_list<double>);
