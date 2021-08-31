#include "ast/AstParser.h"
#include "ast/AstFormatter.h"

#include "InputOptions.h"
#include "OutputOptions.h"
#include "CliApp.h"

#include <unistd.h>

struct Options: InputOptions, OutputOptions, FormatOptions {};

CLI_APP(dump, "parse and dump descriptor in textual format")
{
	Options opts;

	opts.InputOptions::add(this);
	opts.OutputOptions::add(this);
	opts.FormatOptions::add(this);

	if(this->processCommandLine())
	{
		if(opts.output != &std::cout)
		{
			opts.colored = false;
		}
		else if(opts.colored && !isatty(fileno(stdout)))
		{
		   std::cerr << "Output is not a terminal, disabling coloring. Note: the -o option is available to save output to file." << std::endl;
		   opts.colored = false;
		}

		*opts.output << format(opts, parse(*opts.input));
		return 0;
	}

	return -1;
}
