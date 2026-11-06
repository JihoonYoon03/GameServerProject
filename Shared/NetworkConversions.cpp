#include "NetworkConversions.h"
#include <GL/glm/gtc/type_ptr.hpp>

//euler_angles 기능은 아직 실험적 기능이라고 함. 이거 추가해야함.
#define GLM_ENABLE_EXPERIMENTAL
#include <GL/glm/gtx/euler_angles.hpp>

namespace Net
{
    //실수값 0 검사 용도
#define EPSILON 1e-4f

    //-------------------------------------------------------------
    // 
    // 일반 데이터 -> 송수신용 데이터 포맷으로 변경
    // 
    //-------------------------------------------------------------
    Vec3Data ToWire(const glm::vec3& value)
    {
        Vec3Data data{ value.x, value.y, value.z };
        return data;
    }

    Mat4Data ToWire(const glm::mat4& value)
    {
        Mat4Data data;
        //메모리 통째로 카피
        //glm 행렬은 열 우선임. elements에서 접근 시엔 [col * 4 + row] 로 접근할 것
        ::memcpy_s(data.elements.data(), 64, glm::value_ptr(value), 64);

        return data;
    }

    //-------------------------------------------------------------
    // 
    // 송수신용 데이터 포맷 -> 일반 데이터로 변경
    // 
    //-------------------------------------------------------------
    glm::vec3 FromWire(const Vec3Data& value)
    {
        glm::vec3 data{ value.x, value.y, value.z };
        return data;
    }

    glm::mat4 FromWire(const Mat4Data& value)
    {
        glm::mat4 data;
        ::memcpy_s(glm::value_ptr(data), 64, value.elements.data(), 64);
        return data;
    }

    Mat4Data MakePlayerTransform(const glm::vec3& position, float yaw, float pitch)
    {
        //gl 행렬계산은 식 오른쪽부터 곱해짐
        //아래 식은 Translate * RotationY * RotationX가 됨.
        glm::mat4 world(1.0f);
        world = glm::translate(world, position);
        world = glm::rotate(world, glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));
        world = glm::rotate(world, glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));

        return ToWire(world);
    }

    bool floatNotEqual(float val1, float val2)
    {
        return abs(val1 - val2) >= EPSILON;
    }

    bool TryReadPlayerTransform(const Mat4Data& value, glm::vec3& position, float& yaw, float& pitch)
    {
        glm::mat4 m = FromWire(value);

        //길이 0 normalize로 NaN이 생긴 경우가 있는지 검사
        for (float element : value.elements)
        {
            if (!std::isfinite(element))
                return false;
        }

        //아래 검사 조건은 AI 사용
        //마지막 행 값이 0, 0, 0, 1이 아닌지 검사
        if (floatNotEqual(m[0][3], 0) ||
            floatNotEqual(m[1][3], 0) ||
            floatNotEqual(m[2][3], 0) ||
            floatNotEqual(m[3][3], 1))
            return false;

        //각 열(= 회전축을 의미함)의 길이가 1이 아닌지 검사
        if (floatNotEqual(glm::length(glm::vec3(m[0])), 1) ||
            floatNotEqual(glm::length(glm::vec3(m[1])), 1) ||
            floatNotEqual(glm::length(glm::vec3(m[2])), 1))
            return false;

        //회전축 직교 검사
        if (floatNotEqual(glm::dot(glm::vec3(m[0]), glm::vec3(m[1])), 0) ||
            floatNotEqual(glm::dot(glm::vec3(m[1]), glm::vec3(m[2])), 0) ||
            floatNotEqual(glm::dot(glm::vec3(m[2]), glm::vec3(m[0])), 0))
            return false;

        //반사되어 변환되는지 여부
        if (floatNotEqual(glm::determinant(glm::mat3(m)), 1))
            return false;


        //아래는 데이터 추출 과정임
        //마지막 열
        glm::vec3 pos = glm::vec3(m[3]);
        //4행 & 4열 제외
        glm::mat3 mtxRot = glm::mat3(m);

        float exYaw, exPitch, exRoll;
        //해당 함수로 제대로 각도 추출하려면, 생성 시 적용 순서와 일치해야함
        //Translate * RotationY * RotationX였으므로 YXZ 호출
        glm::extractEulerAngleYXZ<float>(m, exYaw, exPitch, exRoll);
        if (floatNotEqual(exRoll, 0))
            return false;

        position = pos;
        yaw = glm::degrees(exYaw);
        pitch = glm::degrees(exPitch);

        return true;
    }
}