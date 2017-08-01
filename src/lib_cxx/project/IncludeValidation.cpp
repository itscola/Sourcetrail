#include "project/IncludeValidation.h"

#include "project/IncludeDirective.h"
#include "utility/file/FilePath.h"
#include "utility/text/TextAccess.h"
#include "utility/utility.h"
#include "utility/utilityString.h"

std::vector<IncludeDirective> IncludeValidation::getUnresolvedIncludeDirectives(
	const std::vector<FilePath>& sourceFilePaths,
	const std::vector<FilePath>& indexedPaths,
	const std::vector<FilePath>& headerSearchDirectories,
	size_t quantileCount, std::function<void(float)> progress
)
{
	struct IncludeDirectiveComparator
	{
		bool operator()(const IncludeDirective& a, const IncludeDirective& b)
		{
			return a.getIncludedFile() < b.getIncludedFile();
		}
	};

	std::set<std::string> processedFilePaths;
	std::set<IncludeDirective, IncludeDirectiveComparator> unresolvedIncludeDirectives;

	quantileCount = std::min(quantileCount, sourceFilePaths.size());

	std::vector<std::vector<FilePath>> quantiles;
	for (size_t i = 0; i < quantileCount; i++)
	{
		quantiles.push_back(std::vector<FilePath>());
	}
	for (size_t i = 0; i < sourceFilePaths.size(); i++)
	{
		quantiles[i%quantileCount].push_back(sourceFilePaths[i]);
	}

	for (size_t i = 0; i < quantiles.size(); i++)
	{
		const std::vector<FilePath>& quantile = quantiles[i];

		progress(float(i) / quantiles.size());

		std::set<FilePath> unprocessedFilePaths(quantile.begin(), quantile.end());

		while (!unprocessedFilePaths.empty())
		{
			std::transform(unprocessedFilePaths.begin(), unprocessedFilePaths.end(), std::inserter(processedFilePaths, processedFilePaths.begin()), [](const FilePath& p){ return p.str(); });
			std::set<FilePath> tempUnprocessedFilePaths;

			for (const FilePath& filePath: unprocessedFilePaths)
			{
				for (const IncludeDirective& includeDirective: getIncludeDirectives(filePath))
				{
					const FilePath resolvedIncludePath = resolveIncludeDirective(includeDirective, headerSearchDirectories);
					if (resolvedIncludePath.empty())
					{
						unresolvedIncludeDirectives.insert(includeDirective);
					}
					else if (processedFilePaths.find(resolvedIncludePath.str()) == processedFilePaths.end())
					{
						for (const FilePath& indexedPath: indexedPaths)
						{
							if (indexedPath.contains(resolvedIncludePath))
							{
								tempUnprocessedFilePaths.insert(resolvedIncludePath);
								break;
							}
						}
					}
				}
			}

			unprocessedFilePaths = tempUnprocessedFilePaths;
		}
	}

	std::vector<IncludeDirective> ret;

	for (const IncludeDirective& directive: unresolvedIncludeDirectives)
	{
		ret.push_back(directive);
	}

	progress(1.0f);

	return ret;
}

std::vector<IncludeDirective> IncludeValidation::getIncludeDirectives(const FilePath& filePath)
{
	std::vector<IncludeDirective> includeDirectives;

	if (filePath.exists())
	{
		std::shared_ptr<TextAccess> textAccess = TextAccess::createFromFile(filePath);
		const std::vector<std::string> lines = textAccess->getAllLines();
		for (size_t i = 0; i < lines.size(); i++)
		{
			const std::string lineTrimmedToHash = utility::trim(lines[i]);
			if (utility::isPrefix("#", lineTrimmedToHash))
			{
				const std::string lineTrimmedToInclude = utility::trim(lineTrimmedToHash.substr(1));
				if (utility::isPrefix("include", lineTrimmedToInclude))
				{
					std::string includeString = utility::substrBetween(lineTrimmedToInclude, "<", ">");
					bool usesBrackets = true;
					if (includeString.empty())
					{
						includeString = utility::substrBetween(lineTrimmedToInclude, "\"", "\"");
						usesBrackets = false;
					}

					if (!includeString.empty())
					{
						// lines are 1 based
						includeDirectives.push_back(IncludeDirective(FilePath(includeString), filePath, i + 1, usesBrackets));
					}
				}
			}
		}
	}

	return includeDirectives;
}

FilePath IncludeValidation::resolveIncludeDirective(const IncludeDirective& includeDirective, const std::vector<FilePath>& headerSearchDirectories)
{
	{
		// check for an absolute include path
		if (includeDirective.getIncludedFile().exists())
		{
			return includeDirective.getIncludedFile();
		}
	}

	{
		// check for an include path relative to the including path
		FilePath resolvedIncludePath = includeDirective.getIncludingFile().parentDirectory().concat(includeDirective.getIncludedFile());
		if (resolvedIncludePath.exists())
		{
			return resolvedIncludePath;
		}
	}

	{
		// check for an include path relative to the header search directories
		for (const FilePath& headerSearchDirectory: headerSearchDirectories)
		{
			FilePath resolvedIncludePath = headerSearchDirectory.concat(includeDirective.getIncludedFile());
			if (resolvedIncludePath.exists())
			{
				return resolvedIncludePath;
			}
		}
	}

	return FilePath();
}