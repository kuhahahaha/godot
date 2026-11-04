#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>
#include <metal_atomic>

using namespace metal;

template<typename T>
[[clang::optnone]] T spvFMul(T l, T r)
{
    return fma(l, r, T(0));
}

template<typename T, int Cols, int Rows>
[[clang::optnone]] vec<T, Cols> spvFMulVectorMatrix(vec<T, Rows> v, matrix<T, Cols, Rows> m)
{
    vec<T, Cols> res = vec<T, Cols>(0);
    for (uint i = Rows; i > 0; --i)
    {
        vec<T, Cols> tmp(0);
        for (uint j = 0; j < Cols; ++j)
        {
            tmp[j] = m[j][i - 1];
        }
        res = fma(tmp, vec<T, Cols>(v[i - 1]), res);
    }
    return res;
}

template<typename T, int Cols, int Rows>
[[clang::optnone]] vec<T, Rows> spvFMulMatrixVector(matrix<T, Cols, Rows> m, vec<T, Cols> v)
{
    vec<T, Rows> res = vec<T, Rows>(0);
    for (uint i = Cols; i > 0; --i)
    {
        res = fma(m[i - 1], vec<T, Rows>(v[i - 1]), res);
    }
    return res;
}

template<typename T, int LCols, int LRows, int RCols, int RRows>
[[clang::optnone]] matrix<T, RCols, LRows> spvFMulMatrixMatrix(matrix<T, LCols, LRows> l, matrix<T, RCols, RRows> r)
{
    matrix<T, RCols, LRows> res;
    for (uint i = 0; i < RCols; i++)
    {
        vec<T, RCols> tmp(0);
        for (uint j = 0; j < LCols; j++)
        {
            tmp = fma(vec<T, RCols>(r[i][j]), l[j], tmp);
        }
        res[i] = tmp;
    }
    return res;
}

template<typename T>
[[clang::optnone]] T spvFAdd(T l, T r)
{
    return fma(T(1), l, r);
}

template<typename T>
[[clang::optnone]] T spvFSub(T l, T r)
{
    return fma(T(-1), r, l);
}

struct GuardScratch
{
    uint words[1];
};

struct PreviousRecords
{
    uint words[1];
};

struct CurrentRecords
{
    uint words[1];
};

struct SurfaceProof
{
    float4 current_mesh_min;
    float4 current_mesh_max;
    float4 current_displacement_min;
    float4 current_displacement_max;
    float4 previous_mesh_min;
    float4 previous_mesh_max;
    float4 previous_displacement_min;
    float4 previous_displacement_max;
    uint4 command;
};

struct GeometryProof
{
    float4 current_decl_min;
    float4 current_decl_max;
    float4 previous_decl_min;
    float4 previous_decl_max;
    float4 cull_min;
    float4 cull_max;
    float4 current_host_inverse_rows[3];
    float4 previous_host_inverse_rows[3];
    uint4 source_accessor;
    uint4 options;
    SurfaceProof surfaces[1];
};

struct Params
{
    uint capacity;
    uint sequence_low;
    uint sequence_high;
    uint surface_count;
    uint record_base;
    uint record_count;
    uint reserved0;
    uint reserved1;
};

struct SurfaceProof_1
{
    float4 current_mesh_min;
    float4 current_mesh_max;
    float4 current_displacement_min;
    float4 current_displacement_max;
    float4 previous_mesh_min;
    float4 previous_mesh_max;
    float4 previous_displacement_min;
    float4 previous_displacement_max;
    uint4 command;
};

struct GuardStatus
{
    uint words[1];
};

constant uint3 gl_WorkGroupSize [[maybe_unused]] = uint3(64u, 1u, 1u);

struct OutputMetadata
{
    uint words[1];
};

struct SourceBacking
{
    uint words[1];
};

struct ProtectedCommands
{
    uint words[1];
};

struct spvDescriptorSetBuffer0
{
    device CurrentRecords* __restrict current_records [[id(0)]];
    device PreviousRecords* __restrict previous_records [[id(1)]];
    device OutputMetadata* __restrict output_meta [[id(2)]];
    device SourceBacking* __restrict source [[id(3)]];
    device GuardScratch* __restrict scratch [[id(4)]];
    device GuardStatus* __restrict status [[id(5)]];
    device ProtectedCommands* __restrict commands [[id(6)]];
    device GeometryProof* __restrict proof [[id(7)]];
    constant uint* spvBufferSizeConstants [[id(8)]];
};

static inline __attribute__((always_inline))
void fault(thread const uint& bits, thread const uint& ordinal, device GuardScratch& __restrict scratch, constant uint& scratchBufferSize)
{
    uint _96 = atomic_fetch_or_explicit((device atomic_uint*)&scratch.words[8], bits, memory_order_relaxed);
    uint _100 = atomic_fetch_min_explicit((device atomic_uint*)&scratch.words[9], ordinal, memory_order_relaxed);
}

static inline __attribute__((always_inline))
uint read_record(thread const uint& base, thread const uint& index, thread const bool& previous, device PreviousRecords& __restrict previous_records, constant uint& previous_recordsBufferSize, device CurrentRecords& __restrict current_records, constant uint& current_recordsBufferSize)
{
    uint _466;
    if (previous)
    {
        _466 = previous_records.words[base + index];
    }
    else
    {
        _466 = current_records.words[base + index];
    }
    return _466;
}

static inline __attribute__((always_inline))
bool finite_normal_or_zero(thread const float& value)
{
    uint magnitude = as_type<uint>(value) & 2147483647u;
    bool _108 = magnitude < 2139095040u;
    bool _117;
    if (_108)
    {
        _117 = (magnitude == 0u) || (magnitude >= 8388608u);
    }
    else
    {
        _117 = _108;
    }
    return _117;
}

static inline __attribute__((always_inline))
float4 read_row(thread const uint& base, thread const uint& offset, thread const bool& previous, device PreviousRecords& __restrict previous_records, constant uint& previous_recordsBufferSize, device CurrentRecords& __restrict current_records, constant uint& current_recordsBufferSize)
{
    uint param = base;
    uint param_1 = offset;
    bool param_2 = previous;
    uint param_3 = base;
    uint param_4 = offset + 1u;
    bool param_5 = previous;
    uint param_6 = base;
    uint param_7 = offset + 2u;
    bool param_8 = previous;
    uint param_9 = base;
    uint param_10 = offset + 3u;
    bool param_11 = previous;
    return as_type<float4>(uint4(read_record(param, param_1, param_2, previous_records, previous_recordsBufferSize, current_records, current_recordsBufferSize), read_record(param_3, param_4, param_5, previous_records, previous_recordsBufferSize, current_records, current_recordsBufferSize), read_record(param_6, param_7, param_8, previous_records, previous_recordsBufferSize, current_records, current_recordsBufferSize), read_record(param_9, param_10, param_11, previous_records, previous_recordsBufferSize, current_records, current_recordsBufferSize)));
}

static inline __attribute__((always_inline))
float next_up(thread const float& value)
{
    uint bits = as_type<uint>(value);
    if ((bits & 2147483647u) == 0u)
    {
        return 1.4012984643248170709237295832899e-45;
    }
    uint _133;
    if (value > 0.0)
    {
        _133 = bits + 1u;
    }
    else
    {
        _133 = bits - 1u;
    }
    return as_type<float>(_133);
}

static inline __attribute__((always_inline))
float next_down(thread const float& value)
{
    float param = -value;
    return -next_up(param);
}

static inline __attribute__((always_inline))
float2 rounded_interval(thread const float& value, thread bool& numeric_valid)
{
    bool _153 = isnan(value);
    bool _159;
    if (!_153)
    {
        _159 = isinf(value);
    }
    else
    {
        _159 = _153;
    }
    if (_159)
    {
        numeric_valid = false;
        return float2(0.0);
    }
    uint exponent = (as_type<uint>(value) >> 23u) & 255u;
    uint _175;
    if (exponent > 23u)
    {
        _175 = (exponent - 23u) << 23u;
    }
    else
    {
        uint _184;
        if (exponent == 0u)
        {
            _184 = 1u;
        }
        else
        {
            _184 = 1u << (exponent - 1u);
        }
        _175 = _184;
    }
    uint spacing_bits = _175;
    float width = as_type<float>(max(spacing_bits, 897581056u));
    float low = spvFSub(value, width);
    float high = spvFAdd(value, width);
    bool _207 = isnan(low);
    bool _213;
    if (!_207)
    {
        _213 = isinf(low);
    }
    else
    {
        _213 = _207;
    }
    bool _219;
    if (!_213)
    {
        _219 = isnan(high);
    }
    else
    {
        _219 = _213;
    }
    bool _225;
    if (!_219)
    {
        _225 = isinf(high);
    }
    else
    {
        _225 = _219;
    }
    if (_225)
    {
        numeric_valid = false;
        return float2(0.0);
    }
    float param = low;
    float param_1 = high;
    float2 result = float2(next_down(param), next_up(param_1));
    bool _240 = any(isnan(result));
    bool _247;
    if (!_240)
    {
        _247 = any(isinf(result));
    }
    else
    {
        _247 = _240;
    }
    if (_247)
    {
        numeric_valid = false;
    }
    return result;
}

static inline __attribute__((always_inline))
float2 add_interval(thread const float2& left, thread const float2& right, thread bool& numeric_valid)
{
    float low = spvFAdd(left.x, right.x);
    float high = spvFAdd(left.y, right.y);
    float param = low;
    bool param_1 = numeric_valid;
    float2 _269 = rounded_interval(param, param_1);
    numeric_valid = param_1;
    float param_2 = high;
    bool param_3 = numeric_valid;
    float2 _276 = rounded_interval(param_2, param_3);
    numeric_valid = param_3;
    return float2(_269.x, _276.y);
}

static inline __attribute__((always_inline))
float2 multiply_interval(thread const float2& left, thread const float2& right, thread bool& numeric_valid)
{
    float p0 = spvFMul(left.x, right.x);
    float p1 = spvFMul(left.x, right.y);
    float p2 = spvFMul(left.y, right.x);
    float p3 = spvFMul(left.y, right.y);
    float param = p0;
    bool param_1 = numeric_valid;
    float2 _311 = rounded_interval(param, param_1);
    numeric_valid = param_1;
    float2 a = _311;
    float param_2 = p1;
    bool param_3 = numeric_valid;
    float2 _318 = rounded_interval(param_2, param_3);
    numeric_valid = param_3;
    float2 b = _318;
    float param_4 = p2;
    bool param_5 = numeric_valid;
    float2 _325 = rounded_interval(param_4, param_5);
    numeric_valid = param_5;
    float2 c = _325;
    float param_6 = p3;
    bool param_7 = numeric_valid;
    float2 _332 = rounded_interval(param_6, param_7);
    numeric_valid = param_7;
    float2 d = _332;
    return float2(fast::min(fast::min(a.x, b.x), fast::min(c.x, d.x)), fast::max(fast::max(a.y, b.y), fast::max(c.y, d.y)));
}

static inline __attribute__((always_inline))
float2 affine_interval(thread const float4& row, thread const float2& x, thread const float2& y, thread const float2& z, thread bool& numeric_valid)
{
    float2 param = float2(row.x);
    float2 param_1 = x;
    bool param_2 = numeric_valid;
    float2 _368 = multiply_interval(param, param_1, param_2);
    numeric_valid = param_2;
    float2 result = _368;
    float2 param_3 = float2(row.y);
    float2 param_4 = y;
    bool param_5 = numeric_valid;
    float2 _378 = multiply_interval(param_3, param_4, param_5);
    numeric_valid = param_5;
    float2 param_6 = result;
    float2 param_7 = _378;
    bool param_8 = numeric_valid;
    float2 _385 = add_interval(param_6, param_7, param_8);
    numeric_valid = param_8;
    result = _385;
    float2 param_9 = float2(row.z);
    float2 param_10 = z;
    bool param_11 = numeric_valid;
    float2 _396 = multiply_interval(param_9, param_10, param_11);
    numeric_valid = param_11;
    float2 param_12 = result;
    float2 param_13 = _396;
    bool param_14 = numeric_valid;
    float2 _403 = add_interval(param_12, param_13, param_14);
    numeric_valid = param_14;
    result = _403;
    float2 param_15 = result;
    float2 param_16 = float2(row.w);
    bool param_17 = numeric_valid;
    float2 _414 = add_interval(param_15, param_16, param_17);
    numeric_valid = param_17;
    return _414;
}

static inline __attribute__((always_inline))
bool contained(thread const float2& x, thread const float2& y, thread const float2& z, thread const float3& lower, thread const float3& upper)
{
    bool _422 = x.x >= lower.x;
    bool _430;
    if (_422)
    {
        _430 = y.x >= lower.y;
    }
    else
    {
        _430 = _422;
    }
    bool _438;
    if (_430)
    {
        _438 = z.x >= lower.z;
    }
    else
    {
        _438 = _430;
    }
    bool _446;
    if (_438)
    {
        _446 = x.y <= upper.x;
    }
    else
    {
        _446 = _438;
    }
    bool _454;
    if (_446)
    {
        _454 = y.y <= upper.y;
    }
    else
    {
        _454 = _446;
    }
    bool _462;
    if (_454)
    {
        _462 = z.y <= upper.z;
    }
    else
    {
        _462 = _454;
    }
    return _462;
}

static inline __attribute__((always_inline))
uint validate_record(thread const uint& ordinal, thread const bool& previous, thread uint& first_failure, device PreviousRecords& __restrict previous_records, constant uint& previous_recordsBufferSize, device CurrentRecords& __restrict current_records, constant uint& current_recordsBufferSize, device GeometryProof& __restrict proof, constant Params& params)
{
    first_failure = 4294967295u;
    uint failure_base = (ordinal * 16u) + (previous ? 8u : 0u);
    uint base = ordinal * 48u;
    uint record_fault = 0u;
    for (uint i = 0u; i < 44u; i++)
    {
        uint param = base;
        uint param_1 = i;
        bool param_2 = previous;
        float param_3 = as_type<float>(read_record(param, param_1, param_2, previous_records, previous_recordsBufferSize, current_records, current_recordsBufferSize));
        if (!finite_normal_or_zero(param_3))
        {
            record_fault |= 16u;
        }
    }
    uint param_4 = base;
    uint param_5 = 46u;
    bool param_6 = previous;
    uint flags = read_record(param_4, param_5, param_6, previous_records, previous_recordsBufferSize, current_records, current_recordsBufferSize);
    bool _579 = (flags & 4294967264u) != 0u;
    bool _586;
    if (!_579)
    {
        _586 = (flags & 1u) == 0u;
    }
    else
    {
        _586 = _579;
    }
    bool _598;
    if (!_586)
    {
        uint param_7 = base;
        uint param_8 = 47u;
        bool param_9 = previous;
        _598 = read_record(param_7, param_8, param_9, previous_records, previous_recordsBufferSize, current_records, current_recordsBufferSize) != 0u;
    }
    else
    {
        _598 = _586;
    }
    bool _610;
    if (!_598)
    {
        uint param_10 = base;
        uint param_11 = 15u;
        bool param_12 = previous;
        _610 = read_record(param_10, param_11, param_12, previous_records, previous_recordsBufferSize, current_records, current_recordsBufferSize) != 0u;
    }
    else
    {
        _610 = _598;
    }
    bool _622;
    if (!_610)
    {
        uint param_13 = base;
        uint param_14 = 19u;
        bool param_15 = previous;
        _622 = read_record(param_13, param_14, param_15, previous_records, previous_recordsBufferSize, current_records, current_recordsBufferSize) != 0u;
    }
    else
    {
        _622 = _610;
    }
    bool _633;
    if (!_622)
    {
        uint param_16 = base;
        uint param_17 = 23u;
        bool param_18 = previous;
        _633 = read_record(param_16, param_17, param_18, previous_records, previous_recordsBufferSize, current_records, current_recordsBufferSize) != 0u;
    }
    else
    {
        _633 = _622;
    }
    if (_633)
    {
        record_fault |= 16u;
    }
    bool _669;
    if (previous)
    {
        uint param_19 = base;
        uint param_20 = 44u;
        bool param_21 = true;
        uint param_22 = base;
        uint param_23 = 44u;
        bool param_24 = false;
        bool _652 = read_record(param_19, param_20, param_21, previous_records, previous_recordsBufferSize, current_records, current_recordsBufferSize) != read_record(param_22, param_23, param_24, previous_records, previous_recordsBufferSize, current_records, current_recordsBufferSize);
        bool _668;
        if (!_652)
        {
            uint param_25 = base;
            uint param_26 = 45u;
            bool param_27 = true;
            uint param_28 = base;
            uint param_29 = 45u;
            bool param_30 = false;
            _668 = read_record(param_25, param_26, param_27, previous_records, previous_recordsBufferSize, current_records, current_recordsBufferSize) != read_record(param_28, param_29, param_30, previous_records, previous_recordsBufferSize, current_records, current_recordsBufferSize);
        }
        else
        {
            _668 = _652;
        }
        _669 = _668;
    }
    else
    {
        _669 = previous;
    }
    if (_669)
    {
        record_fault |= 32u;
    }
    if (record_fault != 0u)
    {
        first_failure = failure_base;
        return record_fault;
    }
    uint param_31 = base;
    uint param_32 = 0u;
    bool param_33 = previous;
    float4 row0 = read_row(param_31, param_32, param_33, previous_records, previous_recordsBufferSize, current_records, current_recordsBufferSize);
    uint param_34 = base;
    uint param_35 = 4u;
    bool param_36 = previous;
    float4 row1 = read_row(param_34, param_35, param_36, previous_records, previous_recordsBufferSize, current_records, current_recordsBufferSize);
    uint param_37 = base;
    uint param_38 = 8u;
    bool param_39 = previous;
    float4 row2 = read_row(param_37, param_38, param_39, previous_records, previous_recordsBufferSize, current_records, current_recordsBufferSize);
    float3 _706;
    if (previous)
    {
        _706 = proof.previous_decl_min.xyz;
    }
    else
    {
        _706 = proof.current_decl_min.xyz;
    }
    float3 declaration_min = _706;
    float3 _728;
    if (previous)
    {
        _728 = proof.previous_decl_max.xyz;
    }
    else
    {
        _728 = proof.current_decl_max.xyz;
    }
    float3 declaration_max = _728;
    SurfaceProof_1 bound;
    float3 _787;
    float3 _800;
    float3 _813;
    float3 _826;
    float _849;
    float _861;
    float _873;
    float4 _991;
    float4 _1002;
    float4 _1013;
    for (uint surface = 0u; surface < params.surface_count; surface++)
    {
        bound.current_mesh_min = proof.surfaces[surface].current_mesh_min;
        bound.current_mesh_max = proof.surfaces[surface].current_mesh_max;
        bound.current_displacement_min = proof.surfaces[surface].current_displacement_min;
        bound.current_displacement_max = proof.surfaces[surface].current_displacement_max;
        bound.previous_mesh_min = proof.surfaces[surface].previous_mesh_min;
        bound.previous_mesh_max = proof.surfaces[surface].previous_mesh_max;
        bound.previous_displacement_min = proof.surfaces[surface].previous_displacement_min;
        bound.previous_displacement_max = proof.surfaces[surface].previous_displacement_max;
        bound.command = proof.surfaces[surface].command;
        if (previous)
        {
            _787 = bound.previous_mesh_min.xyz;
        }
        else
        {
            _787 = bound.current_mesh_min.xyz;
        }
        float3 mesh_min = _787;
        if (previous)
        {
            _800 = bound.previous_mesh_max.xyz;
        }
        else
        {
            _800 = bound.current_mesh_max.xyz;
        }
        float3 mesh_max = _800;
        if (previous)
        {
            _813 = bound.previous_displacement_min.xyz;
        }
        else
        {
            _813 = bound.current_displacement_min.xyz;
        }
        float3 displacement_min = _813;
        if (previous)
        {
            _826 = bound.previous_displacement_max.xyz;
        }
        else
        {
            _826 = bound.current_displacement_max.xyz;
        }
        float3 displacement_max = _826;
        for (uint corner = 0u; corner < 8u; corner++)
        {
            if ((corner & 1u) == 0u)
            {
                _849 = mesh_min.x;
            }
            else
            {
                _849 = mesh_max.x;
            }
            if ((corner & 2u) == 0u)
            {
                _861 = mesh_min.y;
            }
            else
            {
                _861 = mesh_max.y;
            }
            if ((corner & 4u) == 0u)
            {
                _873 = mesh_min.z;
            }
            else
            {
                _873 = mesh_max.z;
            }
            float3 point = float3(_849, _861, _873);
            bool numeric_valid = true;
            float2 param_40 = float2(point.x);
            float2 param_41 = float2(displacement_min.x, displacement_max.x);
            bool param_42 = numeric_valid;
            float2 _897 = add_interval(param_40, param_41, param_42);
            numeric_valid = param_42;
            float2 x = _897;
            float2 param_43 = float2(point.y);
            float2 param_44 = float2(displacement_min.y, displacement_max.y);
            bool param_45 = numeric_valid;
            float2 _912 = add_interval(param_43, param_44, param_45);
            numeric_valid = param_45;
            float2 y = _912;
            float2 param_46 = float2(point.z);
            float2 param_47 = float2(displacement_min.z, displacement_max.z);
            bool param_48 = numeric_valid;
            float2 _927 = add_interval(param_46, param_47, param_48);
            numeric_valid = param_48;
            float2 z = _927;
            float4 param_49 = row0;
            float2 param_50 = x;
            float2 param_51 = y;
            float2 param_52 = z;
            bool param_53 = numeric_valid;
            float2 _940 = affine_interval(param_49, param_50, param_51, param_52, param_53);
            numeric_valid = param_53;
            float2 world_x = _940;
            float4 param_54 = row1;
            float2 param_55 = x;
            float2 param_56 = y;
            float2 param_57 = z;
            bool param_58 = numeric_valid;
            float2 _953 = affine_interval(param_54, param_55, param_56, param_57, param_58);
            numeric_valid = param_58;
            float2 world_y = _953;
            float4 param_59 = row2;
            float2 param_60 = x;
            float2 param_61 = y;
            float2 param_62 = z;
            bool param_63 = numeric_valid;
            float2 _966 = affine_interval(param_59, param_60, param_61, param_62, param_63);
            numeric_valid = param_63;
            float2 world_z = _966;
            float2 param_64 = world_x;
            float2 param_65 = world_y;
            float2 param_66 = world_z;
            float3 param_67 = proof.cull_min.xyz;
            float3 param_68 = proof.cull_max.xyz;
            bool inside_cull = contained(param_64, param_65, param_66, param_67, param_68);
            if (((device uint*)&proof.source_accessor)[3u] == 2u)
            {
                if (previous)
                {
                    _991 = proof.previous_host_inverse_rows[0];
                }
                else
                {
                    _991 = proof.current_host_inverse_rows[0];
                }
                float4 inverse0 = _991;
                if (previous)
                {
                    _1002 = proof.previous_host_inverse_rows[1];
                }
                else
                {
                    _1002 = proof.current_host_inverse_rows[1];
                }
                float4 inverse1 = _1002;
                if (previous)
                {
                    _1013 = proof.previous_host_inverse_rows[2];
                }
                else
                {
                    _1013 = proof.current_host_inverse_rows[2];
                }
                float4 inverse2 = _1013;
                float4 param_69 = inverse0;
                float2 param_70 = world_x;
                float2 param_71 = world_y;
                float2 param_72 = world_z;
                bool param_73 = numeric_valid;
                float2 _1032 = affine_interval(param_69, param_70, param_71, param_72, param_73);
                numeric_valid = param_73;
                x = _1032;
                float4 param_74 = inverse1;
                float2 param_75 = world_x;
                float2 param_76 = world_y;
                float2 param_77 = world_z;
                bool param_78 = numeric_valid;
                float2 _1044 = affine_interval(param_74, param_75, param_76, param_77, param_78);
                numeric_valid = param_78;
                y = _1044;
                float4 param_79 = inverse2;
                float2 param_80 = world_x;
                float2 param_81 = world_y;
                float2 param_82 = world_z;
                bool param_83 = numeric_valid;
                float2 _1056 = affine_interval(param_79, param_80, param_81, param_82, param_83);
                numeric_valid = param_83;
                z = _1056;
            }
            else
            {
                x = world_x;
                y = world_y;
                z = world_z;
            }
            if (!numeric_valid)
            {
                record_fault |= 16u;
                first_failure = min(first_failure, (failure_base + surface));
            }
            else
            {
                bool _1075 = !inside_cull;
                bool _1091;
                if (!_1075)
                {
                    float2 param_84 = x;
                    float2 param_85 = y;
                    float2 param_86 = z;
                    float3 param_87 = declaration_min;
                    float3 param_88 = declaration_max;
                    _1091 = !contained(param_84, param_85, param_86, param_87, param_88);
                }
                else
                {
                    _1091 = _1075;
                }
                if (_1091)
                {
                    record_fault |= 64u;
                    first_failure = min(first_failure, (failure_base + surface));
                }
            }
        }
    }
    return record_fault;
}

kernel void main0(constant spvDescriptorSetBuffer0& spvDescriptorSet0 [[buffer(0)]], constant uint* spvBufferSizeConstants [[buffer(25)]], constant Params& params [[buffer(1)]], uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
{
    constant uint& spvDescriptorSet0_scratchBufferSize = spvDescriptorSet0.spvBufferSizeConstants[4];
    constant uint& spvDescriptorSet0_previous_recordsBufferSize = spvDescriptorSet0.spvBufferSizeConstants[1];
    constant uint& spvDescriptorSet0_current_recordsBufferSize = spvDescriptorSet0.spvBufferSizeConstants[0];
    constant uint& spvDescriptorSet0_statusBufferSize = spvDescriptorSet0.spvBufferSizeConstants[5];
    bool _1112 = uint(int((spvDescriptorSet0_scratchBufferSize - 0) / 4)) < 16u;
    bool _1124;
    if (!_1112)
    {
        _1124 = uint(int((spvDescriptorSet0_statusBufferSize - 0) / 4)) < 8u;
    }
    else
    {
        _1124 = _1112;
    }
    if (_1124)
    {
        return;
    }
    uint lane = gl_GlobalInvocationID.x;
    bool _1139 = params.record_base > params.capacity;
    bool _1151;
    if (!_1139)
    {
        _1151 = params.record_count > (params.capacity - params.record_base);
    }
    else
    {
        _1151 = _1139;
    }
    if (_1151)
    {
        if (lane == 0u)
        {
            uint param = 2u;
            uint param_1 = 0u;
            fault(param, param_1, (*spvDescriptorSet0.scratch), spvDescriptorSet0_scratchBufferSize);
        }
        return;
    }
    if (lane >= params.record_count)
    {
        return;
    }
    uint ordinal = params.record_base + lane;
    bool _1177 = ordinal >= (*spvDescriptorSet0.scratch).words[0];
    bool _1184;
    if (!_1177)
    {
        _1184 = (*spvDescriptorSet0.scratch).words[10] != 0u;
    }
    else
    {
        _1184 = _1177;
    }
    if (_1184)
    {
        return;
    }
    uint current_capacity = uint(int((spvDescriptorSet0_current_recordsBufferSize - 0) / 4)) / 48u;
    if (ordinal >= current_capacity)
    {
        uint param_2 = 2u;
        uint param_3 = ordinal * 16u;
        fault(param_2, param_3, (*spvDescriptorSet0.scratch), spvDescriptorSet0_scratchBufferSize);
        return;
    }
    uint param_4 = ordinal;
    bool param_5 = false;
    uint param_6;
    uint _1210 = validate_record(param_4, param_5, param_6, (*spvDescriptorSet0.previous_records), spvDescriptorSet0_previous_recordsBufferSize, (*spvDescriptorSet0.current_records), spvDescriptorSet0_current_recordsBufferSize, (*spvDescriptorSet0.proof), params);
    uint first_failure = param_6;
    uint bits = _1210;
    uint flags = (*spvDescriptorSet0.current_records).words[(ordinal * 48u) + 46u];
    if ((flags & 2u) != 0u)
    {
        bool _1225 = ((device uint*)&(*spvDescriptorSet0.proof).options)[0u] == 0u;
        bool _1235;
        if (!_1225)
        {
            _1235 = ordinal >= (uint(int((spvDescriptorSet0_previous_recordsBufferSize - 0) / 4)) / 48u);
        }
        else
        {
            _1235 = _1225;
        }
        if (_1235)
        {
            bits |= 32u;
            first_failure = min(first_failure, ((ordinal * 16u) + 8u));
        }
        else
        {
            uint param_7 = ordinal;
            bool param_8 = true;
            uint param_9;
            uint _1251 = validate_record(param_7, param_8, param_9, (*spvDescriptorSet0.previous_records), spvDescriptorSet0_previous_recordsBufferSize, (*spvDescriptorSet0.current_records), spvDescriptorSet0_current_recordsBufferSize, (*spvDescriptorSet0.proof), params);
            uint previous_failure = param_9;
            bits |= _1251;
            first_failure = min(first_failure, previous_failure);
        }
    }
    if (bits != 0u)
    {
        uint param_10 = bits;
        uint param_11 = first_failure;
        fault(param_10, param_11, (*spvDescriptorSet0.scratch), spvDescriptorSet0_scratchBufferSize);
    }
}

