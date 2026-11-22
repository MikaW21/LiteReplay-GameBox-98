#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
namespace lc {
struct PreviewRange {
    int64_t start=0, end=0;
    int64_t seek(double sourceSeconds) const {
        if(!std::isfinite(sourceSeconds)||end<=start) return start;
        long double absolute=static_cast<long double>(sourceSeconds)*10000000;
        return static_cast<int64_t>(std::clamp(absolute,static_cast<long double>(start),static_cast<long double>(end-1)));
    }
    double sourceTime(int64_t absolute) const {
        return std::clamp(absolute,start,end)/10000000.0;
    }
};
inline PreviewRange previewRange(double in,double out,int64_t mediaDuration) {
    if(!std::isfinite(in)||!std::isfinite(out)||in<0||out<=in||mediaDuration<=0)
        throw std::runtime_error("Invalid preview range / 无效的预览范围");
    long double first=static_cast<long double>(in)*10000000;
    long double last=std::min(static_cast<long double>(out)*10000000,static_cast<long double>(mediaDuration));
    if(first>=last||first>=mediaDuration)
        throw std::runtime_error("Preview starts beyond Windows media duration; adjust In / 预览入点超出系统识别时长，请调整入点");
    PreviewRange r{static_cast<int64_t>(first),static_cast<int64_t>(last)};
    if(r.end<=r.start)throw std::runtime_error("Preview range is empty / 预览范围为空");
    return r;
}
}
