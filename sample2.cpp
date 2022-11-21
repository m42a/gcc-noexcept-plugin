void throw_int();

void inner() noexcept
{
	try
	{
		throw_int();
	}
	// This catch ensures that the IP of the call to throw_int is in the call-site table, so the personality doesn't call terminate directly
	catch (void *)
	{
	}
}

void outer()
{
	inner();
}
