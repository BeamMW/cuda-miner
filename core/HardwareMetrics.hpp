#pragma	once

namespace core
{
	struct HardwareMetrics
	{
		struct Value
		{
			unsigned	_max = 0;
			unsigned	_min = 0;
			unsigned	_current = 0;

			Value & operator = (unsigned aValue)
			{
				_current = aValue;
				if (aValue > _max) {
					_max = aValue;
				}
				if (0 == _min) {
					_min = aValue;
				}
				else {
					if (aValue < _min) {
						_min = aValue;
					}
				}
				return *this;
			}

			operator unsigned() const { return _current; }
		};

		Value	t;
		Value	P;
		Value	fan;
	};
}
