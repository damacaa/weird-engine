#pragma once

namespace WeirdEngine
{
	class DebugService
	{
	public:
		DebugService(bool& fly, bool& input)
			: m_fly(fly)
			, m_input(input)
		{
		}

		bool debugFly() const
		{
			return m_fly;
		}

		void setDebugFly(bool value)
		{
			m_fly = value;
		}

		bool debugInput() const
		{
			return m_input;
		}

		void setDebugInput(bool value)
		{
			m_input = value;
		}

	private:
		bool& m_fly;
		bool& m_input;
	};
} // namespace WeirdEngine
