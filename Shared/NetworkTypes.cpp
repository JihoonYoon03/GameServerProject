#include "NetworkTypes.h"

//서버 프로그램에서 클라이언트 목록에 있는 토큰인지 검사를 하기 때문에, 아래 함수는 크게 용도가 없지 않나
bool Net::IsValidSessionToken(const SessionToken& m_sessionToken)
{
	//세션토큰이 비어있거나 / 다 안 채워졌거나 / 값이 0밖에 없는 경우
	if (m_sessionToken.empty() ||
		m_sessionToken.size() != m_sessionToken.max_size() ||
		std::accumulate(m_sessionToken.begin(), m_sessionToken.end(), 0) == 0)
		return false;

	return true;
}

bool Net::IsNewerSequence(Sequence incoming, Sequence previous)
{
	//AI
	Sequence diff = incoming - previous;

	//incoming이 previous보다 순서가 느린 경우(=정상 순서인 경우), diff는 양수임.
	//그 반대인 경우(=비정상 순서), Sequence는 unsigned이므로 diff가 음수 => 매우 큰 양수로 전환
	//0x80000000u는 unsigned int 최대 크기의 절반인데,
	//두 패킷의 순번 차이가 그만큼 날 일은 사실상 없으므로 이렇게 검사함.
	//이렇게 검사하는 이유 -> 순번이 UINT_MAX에서 0으로 다시 회귀하는 부분을 캐치하기 위함
	return diff != 0 && diff < 0x80000000u;
}
