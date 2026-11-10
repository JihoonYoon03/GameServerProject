#include "NetworkConversions.h"

#include <cmath>

bool Net::IsFinitePlayerPose(const PlayerPose& pose)
{
	return std::isfinite(pose.position.x) && std::isfinite(pose.position.y) &&
		std::isfinite(pose.position.z) && std::isfinite(pose.yaw) && std::isfinite(pose.pitch);
}
