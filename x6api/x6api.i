// author: LiuQichun
// date: 2020-03-27

%module x6api
%include std_vector.i
namespace std {
    %template(IntVector) vector<int>;
    %template(DoubleVector) vector<double>;
};
%{
#include "X6api.h"
%}
%include "X6api.h"
