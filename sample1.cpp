#include <exception>
#include <iostream>

struct has_destructor
{
	~has_destructor()
	{
		std::cout << "Unwinding stack\n";
	}
};

void terminate_handler() noexcept
{
	auto ex = std::current_exception();
	if (!ex)
	{
		std::cout << "No exception is being handled in the terminate handler\n";
	}
	else
	{
		try
		{
			std::rethrow_exception(ex);
		}
		catch (int i)
		{
			std::cout << "Handling exception with type=int value=" << i << "\n";
		}
		catch (...)
		{
			std::cout << "Handling exception with unknown type\n";
		}
	}
	std::cout << std::flush;
	std::quick_exit(0);
}

void throw_int()
{
	has_destructor d;
	throw 3;
}

// Indicate that this can throw so the compiler doesn't optimize away the catch in main
void outer();

int main()
{
	std::set_terminate(&terminate_handler);
	try
	{
		outer();
	}
	// This handler would catch the exception, so _Unwind_RaiseException proceeds to phase 2 instead of terminating because there is no handler
	catch (int)
	{
	}
}
