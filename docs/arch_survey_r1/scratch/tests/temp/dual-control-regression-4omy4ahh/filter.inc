struct HandleFilterState
{
	double axis0_filtered = 0.0;
	double axis1_filtered = 0.0;
	bool inited = false;

	void reset(double axis0, double axis1)
	{
		axis0_filtered = axis0;
		axis1_filtered = axis1;
		inited = true;
	}

	void update(double axis0_raw, double axis1_raw, double axis0_alpha, double axis1_alpha)
	{
		if (!inited)
		{
			reset(axis0_raw, axis1_raw);
			return;
		}

		axis0_filtered += axis0_alpha * (axis0_raw - axis0_filtered);
		axis1_filtered += axis1_alpha * (axis1_raw - axis1_filtered);
	}
};

