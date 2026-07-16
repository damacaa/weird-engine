#pragma once

namespace WeirdEngine
{
	struct Dot
	{
		bool isStatic = false;
		unsigned int materialId;

		Dot()
		{
			this->materialId = 0;
		}
	};

	struct UIDot
	{
		bool isStatic = false;
		unsigned int materialId;

		UIDot()
		{
			this->materialId = 0;
		}
	};
} // namespace WeirdEngine
