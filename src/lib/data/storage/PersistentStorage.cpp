#include "data/storage/PersistentStorage.h"

#include <sstream>
#include <queue>

#include "utility/Cache.h"
#include "utility/file/FileInfo.h"
#include "utility/file/FilePath.h"
#include "utility/logging/logging.h"
#include "utility/messaging/type/MessageNewErrors.h"
#include "utility/messaging/type/MessageStatus.h"
#include "utility/text/TextAccess.h"
#include "utility/TimePoint.h"
#include "utility/tracing.h"
#include "utility/utility.h"

#include "data/graph/token_component/TokenComponentAccess.h"
#include "data/graph/token_component/TokenComponentAggregation.h"
#include "data/graph/token_component/TokenComponentFilePath.h"
#include "data/graph/token_component/TokenComponentInheritanceChain.h"
#include "data/graph/Graph.h"
#include "data/location/SourceLocationCollection.h"
#include "data/location/SourceLocationFile.h"
#include "data/parser/AccessKind.h"
#include "data/parser/ParseLocation.h"
#include "settings/ApplicationSettings.h"

PersistentStorage::PersistentStorage(const FilePath& dbPath, const FilePath& bookmarkPath)
	: m_sqliteIndexStorage(dbPath)
	, m_sqliteBookmarkStorage(bookmarkPath)
{
	m_commandIndex.addNode(0, SearchMatch::getCommandName(SearchMatch::COMMAND_ALL));
	m_commandIndex.addNode(0, SearchMatch::getCommandName(SearchMatch::COMMAND_ERROR));
	// m_commandIndex.addNode(0, SearchMatch::getCommandName(SearchMatch::COMMAND_COLOR_SCHEME_TEST));
	m_commandIndex.finishSetup();
}

PersistentStorage::~PersistentStorage()
{
}

Id PersistentStorage::addNode(int type, const std::string& serializedName)
{
	const StorageNode storedNode = m_sqliteIndexStorage.getNodeBySerializedName(serializedName);
	Id id = storedNode.id;

	if (id == 0)
	{
		id = m_sqliteIndexStorage.addNode(type, serializedName);
	}
	else
	{
		if (storedNode.type < type)
		{
			m_sqliteIndexStorage.setNodeType(type, id);
		}
	}

	return id;
}

void PersistentStorage::addFile(const Id id, const std::string& filePath, const std::string& modificationTime, bool complete)
{
	StorageFile file = m_sqliteIndexStorage.getFirstById<StorageFile>(id);
	if (file.id == 0)
	{
		m_sqliteIndexStorage.addFile(id, filePath, modificationTime, complete);
	}
	else if (!file.complete && complete)
	{
		m_sqliteIndexStorage.setFileComplete(complete, id);
	}
}

void PersistentStorage::addSymbol(const Id id, int definitionKind)
{
	if (m_sqliteIndexStorage.getFirstById<StorageSymbol>(id).id == 0)
	{
		m_sqliteIndexStorage.addSymbol(id, definitionKind);
	}
}

Id PersistentStorage::addEdge(int type, Id sourceId, Id targetId)
{
	Id edgeId = m_sqliteIndexStorage.getEdgeBySourceTargetType(sourceId, targetId, type).id;
	if (edgeId == 0)
	{
		edgeId = m_sqliteIndexStorage.addEdge(type, sourceId, targetId);
	}
	return edgeId;
}

Id PersistentStorage::addLocalSymbol(const std::string& name)
{
	Id localSymbolId = m_sqliteIndexStorage.getLocalSymbolByName(name).id;
	if (localSymbolId == 0)
	{
		localSymbolId = m_sqliteIndexStorage.addLocalSymbol(name);
	}
	return localSymbolId;
}

Id PersistentStorage::addSourceLocation(
	Id fileNodeId, uint startLine, uint startCol, uint endLine, uint endCol, int type)
{
	return m_sqliteIndexStorage.addSourceLocation(
		fileNodeId,
		startLine,
		startCol,
		endLine,
		endCol,
		type
	);
}

void PersistentStorage::addOccurrence(Id elementId, Id sourceLocationId)
{
	m_sqliteIndexStorage.addOccurrence(elementId, sourceLocationId);
}

void PersistentStorage::addComponentAccess(Id nodeId , int type)
{
	m_sqliteIndexStorage.addComponentAccess(nodeId, type);
}

void PersistentStorage::addCommentLocation(Id fileNodeId, uint startLine, uint startCol, uint endLine, uint endCol)
{
	m_sqliteIndexStorage.addCommentLocation(
		fileNodeId,
		startLine,
		startCol,
		endLine,
		endCol
	);
}

void PersistentStorage::addError(
	const std::string& message, const FilePath& filePath, uint startLine, uint startCol, bool fatal, bool indexed)
{
	m_sqliteIndexStorage.addError(
		message,
		filePath,
		startLine,
		startCol,
		fatal,
		indexed
	);
}

void PersistentStorage::forEachNode(std::function<void(const StorageNode& /*data*/)> callback) const
{
	for (StorageNode& node: m_sqliteIndexStorage.getAll<StorageNode>())
	{
		callback(node);
	}
}

void PersistentStorage::forEachFile(std::function<void(const StorageFile& /*data*/)> callback) const
{
	for (StorageFile& file: m_sqliteIndexStorage.getAll<StorageFile>())
	{
		callback(file);
	}
}

void PersistentStorage::forEachSymbol(std::function<void(const StorageSymbol& /*data*/)> callback) const
{
	for (StorageSymbol& symbol: m_sqliteIndexStorage.getAll<StorageSymbol>())
	{
		callback(symbol);
	}
}

void PersistentStorage::forEachEdge(std::function<void(const StorageEdge& /*data*/)> callback) const
{
	for (StorageEdge& edge: m_sqliteIndexStorage.getAll<StorageEdge>())
	{
		callback(edge);
	}
}

void PersistentStorage::forEachLocalSymbol(std::function<void(const StorageLocalSymbol& /*data*/)> callback) const
{
	for (StorageLocalSymbol& localSymbol: m_sqliteIndexStorage.getAll<StorageLocalSymbol>())
	{
		callback(localSymbol);
	}
}

void PersistentStorage::forEachSourceLocation(std::function<void(const StorageSourceLocation& /*data*/)> callback) const
{
	for (StorageSourceLocation& sourceLocation: m_sqliteIndexStorage.getAll<StorageSourceLocation>())
	{
		callback(sourceLocation);
	}
}

void PersistentStorage::forEachOccurrence(std::function<void(const StorageOccurrence& /*data*/)> callback) const
{
	for (StorageOccurrence& occurrence: m_sqliteIndexStorage.getAll<StorageOccurrence>())
	{
		callback(occurrence);
	}
}

void PersistentStorage::forEachComponentAccess(std::function<void(const StorageComponentAccess& /*data*/)> callback) const
{
	for (StorageComponentAccess& componentAccess: m_sqliteIndexStorage.getAll<StorageComponentAccess>())
	{
		callback(componentAccess);
	}
}

void PersistentStorage::forEachCommentLocation(std::function<void(const StorageCommentLocation& /*data*/)> callback) const
{
	for (StorageCommentLocation& commentLocation: m_sqliteIndexStorage.getAll<StorageCommentLocation>())
	{
		callback(commentLocation);
	}
}

void PersistentStorage::forEachError(std::function<void(const StorageError& /*data*/)> callback) const
{
	for (StorageError& error: m_sqliteIndexStorage.getAll<StorageError>())
	{
		callback(error);
	}
}

void PersistentStorage::startInjection()
{
	m_preInjectionErrorCount = getErrors().size();

	m_sqliteIndexStorage.beginTransaction();
}

void PersistentStorage::finishInjection()
{
	m_sqliteIndexStorage.commitTransaction();

	auto errors = getErrors();

	if (m_preInjectionErrorCount != errors.size())
	{
		MessageNewErrors(
			std::vector<ErrorInfo>(errors.begin() + m_preInjectionErrorCount, errors.end()),
			getErrorCount(errors)
		).dispatch();
	}
}

void PersistentStorage::setMode(const SqliteIndexStorage::StorageModeType mode)
{
	m_sqliteIndexStorage.setMode(mode);
}

FilePath PersistentStorage::getDbFilePath() const
{
	return m_sqliteIndexStorage.getDbFilePath();
}

bool PersistentStorage::isEmpty() const
{
	return m_sqliteIndexStorage.isEmpty();
}

bool PersistentStorage::isIncompatible() const
{
	return m_sqliteIndexStorage.isIncompatible();
}

std::string PersistentStorage::getProjectSettingsText() const
{
	return m_sqliteIndexStorage.getProjectSettingsText();
}

void PersistentStorage::setProjectSettingsText(std::string text)
{
	m_sqliteIndexStorage.setProjectSettingsText(text);
}

void PersistentStorage::setup()
{
	m_sqliteIndexStorage.setup();
	m_sqliteBookmarkStorage.setup();

	m_sqliteBookmarkStorage.migrateIfNecessary();
}

void PersistentStorage::clear()
{
	m_sqliteIndexStorage.clear();

	clearCaches();
}

void PersistentStorage::clearCaches()
{
	m_symbolIndex.clear();
	m_fileIndex.clear();

	m_fileNodeIds.clear();
	m_fileNodePaths.clear();
	m_fileNodeComplete.clear();
	m_symbolDefinitionKinds.clear();

	m_hierarchyCache.clear();
	m_fullTextSearchIndex.clear();
}

std::set<FilePath> PersistentStorage::getReferenced(const std::set<FilePath>& filePaths)
{
	TRACE();
	std::set<FilePath> referenced;

	utility::append(referenced, getReferencedByIncludes(filePaths));
	utility::append(referenced, getReferencedByImports(filePaths));

	return referenced;
}

std::set<FilePath> PersistentStorage::getReferencing(const std::set<FilePath>& filePaths)
{
	TRACE();
	std::set<FilePath> referencing;

	utility::append(referencing, getReferencingByIncludes(filePaths));
	utility::append(referencing, getReferencingByImports(filePaths));

	return referencing;
}

void PersistentStorage::clearFileElements(const std::vector<FilePath>& filePaths, std::function<void(int)> updateStatusCallback)
{
	TRACE();

	const std::vector<Id> fileNodeIds = getFileNodeIds(filePaths);

	if (!fileNodeIds.empty())
	{
		m_sqliteIndexStorage.beginTransaction();
		m_sqliteIndexStorage.removeElementsWithLocationInFiles(fileNodeIds, updateStatusCallback);
		m_sqliteIndexStorage.removeElements(fileNodeIds);
		m_sqliteIndexStorage.removeErrorsInFiles(filePaths);
		m_sqliteIndexStorage.commitTransaction();
	}
}

std::vector<FileInfo> PersistentStorage::getInfoOnAllFiles() const
{
	TRACE();

	std::vector<FileInfo> fileInfos;

	std::vector<StorageFile> storageFiles = m_sqliteIndexStorage.getAll<StorageFile>();
	for (size_t i = 0; i < storageFiles.size(); i++)
	{
		boost::posix_time::ptime modificationTime = boost::posix_time::not_a_date_time;
		if (storageFiles[i].modificationTime != "not-a-date-time")
		{
			modificationTime = boost::posix_time::time_from_string(storageFiles[i].modificationTime);
		}
		fileInfos.push_back(FileInfo(
			FilePath(storageFiles[i].filePath),
			modificationTime
		));
	}

	return fileInfos;
}

void PersistentStorage::buildCaches()
{
	TRACE();

	clearCaches();

	buildFilePathMaps();
	buildSearchIndex();
	buildHierarchyCache();
}

void PersistentStorage::optimizeMemory()
{
	TRACE();

	m_sqliteIndexStorage.setVersion(m_sqliteIndexStorage.getStaticVersion());
	m_sqliteIndexStorage.setTime();
	m_sqliteIndexStorage.optimizeMemory();

	if (m_sqliteBookmarkStorage.isEmpty())
	{
		m_sqliteBookmarkStorage.setVersion(m_sqliteBookmarkStorage.getStaticVersion());
	}
	m_sqliteBookmarkStorage.optimizeMemory();
}

Id PersistentStorage::getNodeIdForFileNode(const FilePath& filePath) const
{
	return m_sqliteIndexStorage.getFileByPath(filePath.str()).id;
}

Id PersistentStorage::getNodeIdForNameHierarchy(const NameHierarchy& nameHierarchy) const
{
	return m_sqliteIndexStorage.getNodeBySerializedName(NameHierarchy::serialize(nameHierarchy)).id;
}

std::vector<Id> PersistentStorage::getNodeIdsForNameHierarchies(const std::vector<NameHierarchy> nameHierarchies) const
{
	std::vector<Id> nodeIds;
	for (const NameHierarchy& name : nameHierarchies)
	{
		Id nodeId = getNodeIdForNameHierarchy(name);
		if (nodeId)
		{
			nodeIds.push_back(nodeId);
		}
	}
	return nodeIds;
}

NameHierarchy PersistentStorage::getNameHierarchyForNodeId(Id nodeId) const
{
	TRACE();

	return NameHierarchy::deserialize(m_sqliteIndexStorage.getFirstById<StorageNode>(nodeId).serializedName);
}

std::vector<NameHierarchy> PersistentStorage::getNameHierarchiesForNodeIds(const std::vector<Id>& nodeIds) const
{
	TRACE();

	std::vector<NameHierarchy> nameHierarchies;
	for (const StorageNode& storageNode : m_sqliteIndexStorage.getAllByIds<StorageNode>(nodeIds))
	{
		nameHierarchies.push_back(NameHierarchy::deserialize(storageNode.serializedName));
	}
	return nameHierarchies;
}

Node::NodeType PersistentStorage::getNodeTypeForNodeWithId(Id nodeId) const
{
	return Node::intToType(m_sqliteIndexStorage.getFirstById<StorageNode>(nodeId).type);
}

Id PersistentStorage::getIdForEdge(
	Edge::EdgeType type, const NameHierarchy& fromNameHierarchy, const NameHierarchy& toNameHierarchy
) const
{
	Id sourceId = getNodeIdForNameHierarchy(fromNameHierarchy);
	Id targetId = getNodeIdForNameHierarchy(toNameHierarchy);
	return m_sqliteIndexStorage.getEdgeBySourceTargetType(sourceId, targetId, type).id;
}

StorageEdge PersistentStorage::getEdgeById(Id edgeId) const
{
	return m_sqliteIndexStorage.getEdgeById(edgeId);
}

std::shared_ptr<SourceLocationCollection> PersistentStorage::getFullTextSearchLocations(
		const std::string& searchTerm, bool caseSensitive
) const
{
	TRACE();

	std::shared_ptr<SourceLocationCollection> collection = std::make_shared<SourceLocationCollection>();
	if (!searchTerm.size())
	{
		return collection;
	}

	if (m_fullTextSearchIndex.fileCount() == 0)
	{
		MessageStatus("Building fulltext search index", false, true).dispatch();
		buildFullTextSearchIndex();
	}

	MessageStatus(
		std::string("Searching fulltext (case-") + (caseSensitive ? "sensitive" : "insensitive") + "): " + searchTerm,
		false, true
	).dispatch();

	std::vector<FullTextSearchResult> hits = m_fullTextSearchIndex.searchForTerm(searchTerm);
	int termLength = searchTerm.length();

	for (size_t i = 0; i < hits.size(); i++)
	{
		FilePath filePath = getFileNodePath(hits[i].fileId);
		std::shared_ptr<TextAccess> fileContent = getFileContent(filePath);

		int charsInPreviousLines = 0;
		int lineNumber = 1;
		std::string line;
		line = fileContent->getLine(lineNumber);

		for (int pos : hits[i].positions)
		{
			bool addHit = true;
			while( (charsInPreviousLines + (int)line.length()) < pos)
			{
				lineNumber++;
				charsInPreviousLines += line.length();
				line = fileContent->getLine(lineNumber);
			}

			ParseLocation location;
			location.startLineNumber = lineNumber;
			location.startColumnNumber = pos - charsInPreviousLines + 1;

			if ( caseSensitive )
			{
				if( line.substr(location.startColumnNumber-1, termLength) != searchTerm )
				{
					addHit = false;
				}
			}

			while( (charsInPreviousLines + (int)line.length()) < pos + termLength)
			{
				lineNumber++;
				charsInPreviousLines += line.length();
				line = fileContent->getLine(lineNumber);
			}

			location.endLineNumber = lineNumber;
			location.endColumnNumber = pos + termLength - charsInPreviousLines;

			if ( addHit )
			{
				// Set first bit to 1 to avoid collisions
				Id locationId = ~(~size_t(0) >> 1) + collection->getSourceLocationCount();

				collection->addSourceLocation(
					LOCATION_FULLTEXT,
					locationId,
					std::vector<Id>(),
					filePath,
					location.startLineNumber,
					location.startColumnNumber,
					location.endLineNumber,
					location.endColumnNumber
				);
			}
		}
	}

	addCompleteFlagsToSourceLocationCollection(collection.get());

	MessageStatus(
		std::to_string(collection->getSourceLocationCount()) + " results in " +
			std::to_string(collection->getSourceLocationFileCount()) + " files for fulltext search (case-" +
			(caseSensitive ? "sensitive" : "insensitive") + "): " + searchTerm,
		false, false
	).dispatch();

	return collection;
}

std::vector<SearchMatch> PersistentStorage::getAutocompletionMatches(const std::string& query) const
{
	TRACE();

	// search in indices
	size_t maxResultsCount = 100;
	size_t maxBestScoredResultsLength = 100;

	// create SearchMatches
	std::vector<SearchMatch> matches;
	utility::append(matches, getAutocompletionSymbolMatches(query, maxResultsCount));
	utility::append(matches, getAutocompletionFileMatches(query, maxResultsCount));
	utility::append(matches, getAutocompletionCommandMatches(query));

	std::set<SearchMatch> matchesSet;
	for (SearchMatch& match : matches)
	{
		// rescore match
		if (!match.subtext.empty() && match.indices.size())
		{
			SearchResult newResult =
				SearchIndex::rescoreText(match.name, match.text, match.indices, match.score, maxBestScoredResultsLength);

			match.score = newResult.score;
			match.indices = newResult.indices;
		}

		matchesSet.insert(match);
	}

	return utility::toVector(matchesSet);
}

std::vector<SearchMatch> PersistentStorage::getAutocompletionSymbolMatches(
	const std::string& query, size_t maxResultsCount) const
{
	// search in indices
	std::vector<SearchResult> results = m_symbolIndex.search(query, maxResultsCount, maxResultsCount);

	// fetch StorageNodes for node ids
	std::map<Id, StorageNode> storageNodeMap;
	std::map<Id, StorageSymbol> storageSymbolMap;
	{
		std::vector<Id> elementIds;

		for (const SearchResult& result : results)
		{
			elementIds.insert(elementIds.end(), result.elementIds.begin(), result.elementIds.end());
		}

		for (StorageNode& node : m_sqliteIndexStorage.getAllByIds<StorageNode>(elementIds))
		{
			storageNodeMap[node.id] = node;
		}

		for (StorageSymbol& symbol : m_sqliteIndexStorage.getAllByIds<StorageSymbol>(elementIds))
		{
			storageSymbolMap[symbol.id] = symbol;
		}
	}

	// create SearchMatches
	std::vector<SearchMatch> matches;
	for (const SearchResult& result : results)
	{
		SearchMatch match;
		const StorageNode* firstNode = nullptr;

		for (const Id& elementId : result.elementIds)
		{
			if (elementId != 0)
			{
				match.tokenIds.push_back(elementId);

				if (!match.hasChildren)
				{
					match.hasChildren = m_hierarchyCache.nodeHasChildren(elementId);
				}

				if (!firstNode)
				{
					firstNode = &storageNodeMap[elementId];
				}
			}
		}

		match.name = result.text;
		match.text = result.text;

		const size_t idx = m_hierarchyCache.getIndexOfLastVisibleParentNode(firstNode->id);
		const NameHierarchy& name = NameHierarchy::deserialize(firstNode->serializedName);
		match.text = name.getRange(idx, name.size()).getQualifiedName();
		match.subtext = name.getRange(0, idx).getQualifiedName();

		match.delimiter = name.getDelimiter();

		match.indices = result.indices;
		match.score = result.score;
		match.nodeType = Node::intToType(firstNode->type);
		match.typeName = Node::getReadableTypeString(match.nodeType);
		match.searchType = SearchMatch::SEARCH_TOKEN;

		if (storageSymbolMap.find(firstNode->id) == storageSymbolMap.end() &&
			match.nodeType != Node::NODE_NON_INDEXED)
		{
			match.typeName = "non-indexed " + match.typeName;
		}

		matches.push_back(match);
	}

	return matches;
}

std::vector<SearchMatch> PersistentStorage::getAutocompletionFileMatches(const std::string& query, size_t maxResultsCount) const
{
	std::vector<SearchResult> results = m_fileIndex.search(query, maxResultsCount);

	// create SearchMatches
	std::vector<SearchMatch> matches;
	for (const SearchResult& result : results)
	{
		SearchMatch match;

		match.name = result.text;
		match.tokenIds = utility::toVector(result.elementIds);

		FilePath path(match.name);
		match.text = path.fileName();
		match.subtext = path.str();

		match.delimiter = NAME_DELIMITER_FILE;

		match.indices = result.indices;
		match.score = result.score;

		match.nodeType = Node::NODE_FILE;
		match.typeName = Node::getReadableTypeString(match.nodeType);

		match.searchType = SearchMatch::SEARCH_TOKEN;

		matches.push_back(match);
	}

	return matches;
}

std::vector<SearchMatch> PersistentStorage::getAutocompletionCommandMatches(const std::string& query) const
{
	// search in indices
	std::vector<SearchResult> results = m_commandIndex.search(query, 0);

	// create SearchMatches
	std::vector<SearchMatch> matches;
	for (const SearchResult& result : results)
	{
		SearchMatch match;

		match.name = result.text;
		match.text = result.text;

		match.delimiter = NAME_DELIMITER_UNKNOWN;

		match.indices = result.indices;
		match.score = result.score;

		match.searchType = SearchMatch::SEARCH_COMMAND;
		match.typeName = "command";

		matches.push_back(match);
	}

	return matches;
}

std::vector<SearchMatch> PersistentStorage::getSearchMatchesForTokenIds(const std::vector<Id>& elementIds) const
{
	TRACE();

	// todo: what if all these elements share the same node in the searchindex?
	// In that case there should be only one search match.
	std::vector<SearchMatch> matches;

	// fetch StorageNodes for node ids
	std::map<Id, StorageNode> storageNodeMap;
	for (StorageNode& node : m_sqliteIndexStorage.getAllByIds<StorageNode>(elementIds))
	{
		storageNodeMap.emplace(node.id, node);
	}

	for (Id elementId : elementIds)
	{
		if (storageNodeMap.find(elementId) == storageNodeMap.end())
		{
			continue;
		}

		StorageNode node = storageNodeMap[elementId];

		SearchMatch match;
		NameHierarchy nameHierarchy = NameHierarchy::deserialize(node.serializedName);
		match.name = nameHierarchy.getQualifiedName();
		match.text = nameHierarchy.getRawName();

		match.tokenIds.push_back(elementId);
		match.nodeType = Node::intToType(node.type);
		match.searchType = SearchMatch::SEARCH_TOKEN;

		match.delimiter = nameHierarchy.getDelimiter();

		if (match.nodeType == Node::NODE_FILE)
		{
			match.text = FilePath(match.text).fileName();
		}

		matches.push_back(match);
	}

	return matches;
}

std::shared_ptr<Graph> PersistentStorage::getGraphForAll() const
{
	TRACE();

	std::shared_ptr<Graph> graph = std::make_shared<Graph>();

	std::vector<Id> tokenIds;
	for (StorageNode node: m_sqliteIndexStorage.getAll<StorageNode>())
	{
		auto it = m_symbolDefinitionKinds.find(node.id);
		if (it != m_symbolDefinitionKinds.end() && it->second == DEFINITION_EXPLICIT &&
			(
				Node::intToType(node.type) & (Node::NODE_NAMESPACE | Node::NODE_PACKAGE) ||
				!m_hierarchyCache.isChildOfVisibleNodeOrInvisible(node.id)
			)
		){
			tokenIds.push_back(node.id);
		}
	}

	for (auto p : m_fileNodePaths)
	{
		tokenIds.push_back(p.first);
	}

	addNodesToGraph(tokenIds, graph.get(), false);

	return graph;
}

std::shared_ptr<Graph> PersistentStorage::getGraphForActiveTokenIds(
	const std::vector<Id>& tokenIds, const std::vector<Id>& expandedNodeIds, bool* isActiveNamespace) const
{
	TRACE();

	std::vector<Id> ids(tokenIds);
	bool isNamespace = false;

	std::vector<Id> nodeIds;
	std::vector<Id> edgeIds;

	bool addAggregations = false;
	std::vector<StorageEdge> edgesToAggregate;

	if (tokenIds.size() == 1)
	{
		const Id elementId = tokenIds[0];
		StorageNode node = m_sqliteIndexStorage.getFirstById<StorageNode>(elementId);

		if (node.id > 0)
		{
			Node::NodeType nodeType = Node::intToType(node.type);
			if (nodeType & (Node::NODE_NAMESPACE | Node::NODE_PACKAGE))
			{
				ids.clear();
				m_hierarchyCache.addFirstNonImplicitChildIdsForNodeId(elementId, &ids, &edgeIds);
				edgeIds.clear();

				isNamespace = true;
			}
			else
			{
				nodeIds.push_back(elementId);
				m_hierarchyCache.addFirstNonImplicitChildIdsForNodeId(elementId, &nodeIds, &edgeIds);
				edgeIds.clear();

				std::vector<StorageEdge> edges = m_sqliteIndexStorage.getEdgesBySourceOrTargetId(elementId);
				for (const StorageEdge& edge : edges)
				{
					Edge::EdgeType edgeType = Edge::intToType(edge.type);
					if (edgeType == Edge::EDGE_MEMBER)
					{
						continue;
					}

					if ((nodeType & Node::NODE_USEABLE_TYPE) && (edgeType & Edge::EDGE_TYPE_USAGE) &&
						m_hierarchyCache.isChildOfVisibleNodeOrInvisible(edge.sourceNodeId))
					{
						edgesToAggregate.push_back(edge);
					}
					else
					{
						edgeIds.push_back(edge.id);
					}
				}

				addAggregations = true;
			}
		}
		else if (m_sqliteIndexStorage.isEdge(elementId))
		{
			edgeIds.push_back(elementId);
		}
	}

	if (ids.size() >= 1 || isNamespace)
	{
		std::set<Id> symbolIds;
		for (const StorageSymbol& symbol : m_sqliteIndexStorage.getAllByIds<StorageSymbol>(ids))
		{
			if (symbol.id > 0 && (!isNamespace || intToDefinitionKind(symbol.definitionKind) != DEFINITION_IMPLICIT))
			{
				nodeIds.push_back(symbol.id);
			}
			symbolIds.insert(symbol.id);
		}
		for (const StorageNode& node : m_sqliteIndexStorage.getAllByIds<StorageNode>(ids))
		{
			if (symbolIds.find(node.id) == symbolIds.end())
			{
				nodeIds.push_back(node.id);
			}
		}

		if (!isNamespace)
		{
			if (nodeIds.size() != ids.size())
			{
				std::vector<StorageEdge> edges = m_sqliteIndexStorage.getAllByIds<StorageEdge>(ids);
				for (const StorageEdge& edge : edges)
				{
					if (edge.id > 0)
					{
						edgeIds.push_back(edge.id);
					}
				}
			}
		}
	}

	std::shared_ptr<Graph> g = std::make_shared<Graph>();
	Graph* graph = g.get();

	if (isNamespace)
	{
		addNodesToGraph(nodeIds, graph, false);
	}
	else
	{
		addNodesWithParentsAndEdgesToGraph(nodeIds, edgeIds, graph);
	}

	if (addAggregations)
	{
		addAggregationEdgesToGraph(tokenIds[0], edgesToAggregate, graph);
	}

	if (!isNamespace)
	{
		std::vector<Id> expandedChildIds;
		std::vector<Id> expandedChildEdgeIds;

		for (Id nodeId : expandedNodeIds)
		{
			if (graph->getNodeById(nodeId))
			{
				m_hierarchyCache.addFirstNonImplicitChildIdsForNodeId(nodeId, &expandedChildIds, &expandedChildEdgeIds);
			}
		}

		if (expandedChildIds.size())
		{
			addNodesToGraph(expandedChildIds, graph, true);
			addEdgesToGraph(expandedChildEdgeIds, graph);
		}

		addInheritanceChainsToGraph(nodeIds, graph);
	}

	addComponentAccessToGraph(graph);

	if (isActiveNamespace)
	{
		*isActiveNamespace = isNamespace;
	}

	return g;
}

std::shared_ptr<Graph> PersistentStorage::getGraphForChildrenOfNodeId(Id nodeId) const
{
	TRACE();

	std::vector<Id> nodeIds;
	std::vector<Id> edgeIds;

	nodeIds.push_back(nodeId);
	m_hierarchyCache.addFirstNonImplicitChildIdsForNodeId(nodeId, &nodeIds, &edgeIds);

	std::shared_ptr<Graph> graph = std::make_shared<Graph>();
	addNodesToGraph(nodeIds, graph.get(), true);
	addEdgesToGraph(edgeIds, graph.get());

	addComponentAccessToGraph(graph.get());
	return graph;
}

std::shared_ptr<Graph> PersistentStorage::getGraphForTrail(
	Id originId, Id targetId, Edge::EdgeTypeMask trailType, size_t depth) const
{
	TRACE();

	std::set<Id> nodeIds;
	std::set<Id> edgeIds;

	std::vector<Id> nodeIdsToProcess;
	nodeIdsToProcess.push_back(originId ? originId : targetId);
	bool forward = originId;
	size_t currentDepth = 0;

	nodeIds.insert(nodeIdsToProcess.back());
	while (nodeIdsToProcess.size() && (!depth || currentDepth < depth))
	{
		std::vector<StorageEdge> edges =
			forward ?
			m_sqliteIndexStorage.getEdgesBySourceIds(nodeIdsToProcess) :
			m_sqliteIndexStorage.getEdgesByTargetIds(nodeIdsToProcess);

		if (trailType & (Edge::EDGE_OVERRIDE | Edge::EDGE_INHERITANCE))
		{
			utility::append(edges,
				forward ?
				m_sqliteIndexStorage.getEdgesByTargetIds(nodeIdsToProcess) :
				m_sqliteIndexStorage.getEdgesBySourceIds(nodeIdsToProcess)
			);
		}

		nodeIdsToProcess.clear();

		for (const StorageEdge& edge : edges)
		{
			if (Edge::intToType(edge.type) & trailType)
			{
				bool isForward = forward == !(Edge::intToType(edge.type) & (Edge::EDGE_OVERRIDE | Edge::EDGE_INHERITANCE));

				Id nodeId = isForward ? edge.targetNodeId : edge.sourceNodeId;
				Id otherNodeId = isForward ? edge.sourceNodeId : edge.targetNodeId;

				if (nodeIds.find(nodeId) == nodeIds.end())
				{
					nodeIdsToProcess.push_back(nodeId);
					nodeIds.insert(nodeId);
					edgeIds.insert(edge.id);
				}
				else if (nodeIds.find(otherNodeId) != nodeIds.end())
				{
					edgeIds.insert(edge.id);
				}
			}
		}

		currentDepth++;
	}

	std::shared_ptr<Graph> graph = std::make_shared<Graph>();

	addNodesWithParentsAndEdgesToGraph(utility::toVector(nodeIds), utility::toVector(edgeIds), graph.get());
	addComponentAccessToGraph(graph.get());

	return graph;
}

// TODO: rename: getActiveElementIdsForId; TODO: make separate function for declarationId
std::vector<Id> PersistentStorage::getActiveTokenIdsForId(Id tokenId, Id* declarationId) const
{
	TRACE();

	std::vector<Id> activeTokenIds;

	if (!(m_sqliteIndexStorage.isEdge(tokenId) || m_sqliteIndexStorage.isNode(tokenId)))
	{
		return activeTokenIds;
	}

	activeTokenIds.push_back(tokenId);

	if (m_sqliteIndexStorage.isNode(tokenId))
	{
		*declarationId = tokenId;

		std::vector<StorageEdge> incomingEdges = m_sqliteIndexStorage.getEdgesByTargetId(tokenId);
		for (size_t i = 0; i < incomingEdges.size(); i++)
		{
			activeTokenIds.push_back(incomingEdges[i].id);
		}
	}

	return activeTokenIds;
}

std::vector<Id> PersistentStorage::getNodeIdsForLocationIds(const std::vector<Id>& locationIds) const
{
	TRACE();

	std::set<Id> edgeIds;
	std::set<Id> nodeIds;
	std::set<Id> implicitNodeIds;

	for (const StorageOccurrence& occurrence: m_sqliteIndexStorage.getOccurrencesForLocationIds(locationIds))
	{
		const Id elementId = occurrence.elementId;

		StorageEdge edge = m_sqliteIndexStorage.getFirstById<StorageEdge>(elementId);
		if (edge.id != 0) // here we test if location is an edge.
		{
			edgeIds.insert(edge.targetNodeId);
		}
		else if(m_sqliteIndexStorage.isNode(elementId))
		{
			StorageSymbol symbol = m_sqliteIndexStorage.getFirstById<StorageSymbol>(elementId);
			if (symbol.id != 0) // here we test if location is a symbol
			{
				if (intToDefinitionKind(symbol.definitionKind) == DEFINITION_IMPLICIT)
				{
					implicitNodeIds.insert(elementId);
				}
				else
				{
					nodeIds.insert(elementId);
				}
			}
			else // is file
			{
				nodeIds.insert(elementId);
			}
		}
	}

	if (nodeIds.size() == 0)
	{
		nodeIds = implicitNodeIds;
	}

	if (nodeIds.size())
	{
		return utility::toVector(nodeIds);
	}

	return utility::toVector(edgeIds);
}

std::shared_ptr<SourceLocationCollection> PersistentStorage::getSourceLocationsForTokenIds(
	const std::vector<Id>& tokenIds) const
{
	TRACE();

	std::vector<FilePath> filePaths;
	std::vector<Id> nonFileIds;

	for (const Id tokenId : tokenIds)
	{
		FilePath path = getFileNodePath(tokenId);
		if (path.empty())
		{
			nonFileIds.push_back(tokenId);
		}
		else
		{
			filePaths.push_back(path);
		}
	}

	std::shared_ptr<SourceLocationCollection> collection = std::make_shared<SourceLocationCollection>();
	for (const FilePath& path : filePaths)
	{
		collection->addSourceLocationFile(std::make_shared<SourceLocationFile>(path, true, false));
	}

	if (nonFileIds.size())
	{
		std::vector<Id> locationIds;
		std::unordered_map<Id, Id> locationIdToElementIdMap;
		for (const StorageOccurrence& occurrence: m_sqliteIndexStorage.getOccurrencesForElementIds(nonFileIds))
		{
			locationIds.push_back(occurrence.sourceLocationId);
			locationIdToElementIdMap[occurrence.sourceLocationId] = occurrence.elementId;
		}

		for (const StorageSourceLocation& sourceLocation: m_sqliteIndexStorage.getAllByIds<StorageSourceLocation>(locationIds))
		{
			auto it = locationIdToElementIdMap.find(sourceLocation.id);
			if (it != locationIdToElementIdMap.end())
			{
				collection->addSourceLocation(
					intToLocationType(sourceLocation.type),
					sourceLocation.id,
					std::vector<Id>(1, it->second),
					getFileNodePath(sourceLocation.fileNodeId),
					sourceLocation.startLine,
					sourceLocation.startCol,
					sourceLocation.endLine,
					sourceLocation.endCol
				);
			}
		}
	}

	addCompleteFlagsToSourceLocationCollection(collection.get());

	return collection;
}

std::shared_ptr<SourceLocationCollection> PersistentStorage::getSourceLocationsForLocationIds(
		const std::vector<Id>& locationIds
) const
{
	TRACE();

	std::shared_ptr<SourceLocationCollection> collection = std::make_shared<SourceLocationCollection>();

	for (StorageSourceLocation location: m_sqliteIndexStorage.getAllByIds<StorageSourceLocation>(locationIds))
	{
		std::vector<Id> elementIds;
		for (const StorageOccurrence& occurrence: m_sqliteIndexStorage.getOccurrencesForLocationId(location.id))
		{
			elementIds.push_back(occurrence.elementId);
		}

		collection->addSourceLocation(
			intToLocationType(location.type),
			location.id,
			elementIds,
			getFileNodePath(location.fileNodeId),
			location.startLine,
			location.startCol,
			location.endLine,
			location.endCol
		);
	}

	addCompleteFlagsToSourceLocationCollection(collection.get());

	return collection;
}

std::shared_ptr<SourceLocationFile> PersistentStorage::getSourceLocationsForFile(const FilePath& filePath) const
{
	TRACE();

	return m_sqliteIndexStorage.getSourceLocationsForFile(filePath);
}

std::shared_ptr<SourceLocationFile> PersistentStorage::getSourceLocationsForLinesInFile(
		const FilePath& filePath, uint firstLineNumber, uint lastLineNumber
) const
{
	TRACE();

	return getSourceLocationsForFile(filePath)->getFilteredByLines(firstLineNumber, lastLineNumber);
}

std::shared_ptr<SourceLocationFile> PersistentStorage::getCommentLocationsInFile(const FilePath& filePath) const
{
	TRACE();

	std::shared_ptr<SourceLocationFile> file = std::make_shared<SourceLocationFile>(filePath, false, false);

	std::vector<StorageCommentLocation> storageLocations = m_sqliteIndexStorage.getCommentLocationsInFile(filePath);
	for (size_t i = 0; i < storageLocations.size(); i++)
	{
		file->addSourceLocation(
			LOCATION_TOKEN,
			storageLocations[i].id,
			std::vector<Id>(), // comment token location has no element.
			storageLocations[i].startLine,
			storageLocations[i].startCol,
			storageLocations[i].endLine,
			storageLocations[i].endCol
		);
	}

	return file;
}

std::shared_ptr<TextAccess> PersistentStorage::getFileContent(const FilePath& filePath) const
{
	TRACE();

	return m_sqliteIndexStorage.getFileContentByPath(filePath.str());
}

FileInfo PersistentStorage::getFileInfoForFilePath(const FilePath& filePath) const
{
	return FileInfo(filePath, m_sqliteIndexStorage.getFileByPath(filePath.str()).modificationTime);
}

std::vector<FileInfo> PersistentStorage::getFileInfosForFilePaths(const std::vector<FilePath>& filePaths) const
{
	std::vector<FileInfo> fileInfos;

	std::vector<StorageFile> storageFiles = m_sqliteIndexStorage.getFilesByPaths(filePaths);
	for (const StorageFile& file : storageFiles)
	{
		fileInfos.push_back(FileInfo(FilePath(file.filePath), file.modificationTime));
	}

	return fileInfos;
}

StorageStats PersistentStorage::getStorageStats() const
{
	TRACE();

	StorageStats stats;

	stats.nodeCount = m_sqliteIndexStorage.getNodeCount();
	stats.edgeCount = m_sqliteIndexStorage.getEdgeCount();

	stats.fileCount = m_sqliteIndexStorage.getFileCount();
	stats.completedFileCount = m_sqliteIndexStorage.getCompletedFileCount();
	stats.fileLOCCount = m_sqliteIndexStorage.getFileLineSum();

	stats.timestamp = m_sqliteIndexStorage.getTime();

	return stats;
}

ErrorCountInfo PersistentStorage::getErrorCount() const
{
	return getErrorCount(getErrors());
}

ErrorCountInfo PersistentStorage::getErrorCount(const std::vector<ErrorInfo>& errors) const
{
	ErrorCountInfo info;

	for (const ErrorInfo& error : errors)
	{
		info.total++;

		if (error.fatal)
		{
			info.fatal++;
		}
	}

	return info;
}

std::vector<ErrorInfo> PersistentStorage::getErrors() const
{
	std::vector<ErrorInfo> errors;

	for (const ErrorInfo& error : m_sqliteIndexStorage.getAll<StorageError>())
	{
		if (m_errorFilter.filter(error))
		{
			errors.push_back(error);
		}
	}

	return errors;
}

std::vector<ErrorInfo> PersistentStorage::getErrorsLimited() const
{
	std::vector<ErrorInfo> errors;

	for (const ErrorInfo& error : m_sqliteIndexStorage.getAll<StorageError>())
	{
		if (m_errorFilter.filter(error))
		{
			errors.push_back(error);
		}

		if (m_errorFilter.limit > 0 && errors.size() >= m_errorFilter.limit)
		{
			break;
		}
	}

	return errors;
}

std::shared_ptr<SourceLocationCollection> PersistentStorage::getErrorSourceLocationsLimited(std::vector<ErrorInfo>* errors) const
{
	TRACE();

	std::shared_ptr<SourceLocationCollection> collection = std::make_shared<SourceLocationCollection>();
	for (const ErrorInfo& error : m_sqliteIndexStorage.getAll<StorageError>())
	{
		if (m_errorFilter.filter(error))
		{
			errors->push_back(error);

			// Set first bit to 1 to avoid collisions
			Id locationId = ~(~size_t(0) >> 1) + error.id;

			collection->addSourceLocation(
				LOCATION_ERROR,
				locationId,
				std::vector<Id>(1, error.id),
				error.filePath,
				error.lineNumber,
				error.columnNumber,
				error.lineNumber,
				error.columnNumber
			);
		}

		if (m_errorFilter.limit > 0 && errors->size() >= m_errorFilter.limit)
		{
			break;
		}
	}

	addCompleteFlagsToSourceLocationCollection(collection.get());

	return collection;
}

Id PersistentStorage::addNodeBookmark(const NodeBookmark& bookmark)
{
	const Id categoryId = addBookmarkCategory(bookmark.getCategory().getName());
	const Id id = m_sqliteBookmarkStorage.addBookmark(
		bookmark.getName(), bookmark.getComment(), bookmark.getTimeStamp().toString(), categoryId);

	for (const Id& nodeId: bookmark.getNodeIds())
	{
		m_sqliteBookmarkStorage.addBookmarkedNode(id, m_sqliteIndexStorage.getNodeById(nodeId).serializedName);
	}

	return id;
}

Id PersistentStorage::addEdgeBookmark(const EdgeBookmark& bookmark)
{
	const Id categoryId = addBookmarkCategory(bookmark.getCategory().getName());
	const Id id = m_sqliteBookmarkStorage.addBookmark(
		bookmark.getName(), bookmark.getComment(), bookmark.getTimeStamp().toString(), categoryId);
	for (const Id& edgeId: bookmark.getEdgeIds())
	{
		const StorageEdge storageEdge = m_sqliteIndexStorage.getEdgeById(edgeId);

		bool sourceNodeActive = storageEdge.sourceNodeId == bookmark.getActiveNodeId();
		m_sqliteBookmarkStorage.addBookmarkedEdge(
			id,
			// todo: optimization for multiple edges in same bookmark: use a local cache here
			m_sqliteIndexStorage.getNodeById(storageEdge.sourceNodeId).serializedName,
			m_sqliteIndexStorage.getNodeById(storageEdge.targetNodeId).serializedName,
			storageEdge.type,
			sourceNodeActive
		);
	}
	return id;
}

Id PersistentStorage::addBookmarkCategory(const std::string& name)
{
	if (name.empty())
	{
		return 0;
	}

	Id id = m_sqliteBookmarkStorage.getBookmarkCategoryByName(name).id;
	if (id == 0)
	{
		id = m_sqliteBookmarkStorage.addBookmarkCategory(name);
	}
	return id;
}

void PersistentStorage::updateBookmark(
	const Id bookmarkId, const std::string& name, const std::string& comment, const std::string& categoryName)
{
	const Id categoryId = addBookmarkCategory(categoryName); // only creates category if id didn't exist before;
	m_sqliteBookmarkStorage.updateBookmark(bookmarkId, name, comment, categoryId);
}

void PersistentStorage::removeBookmark(const Id id)
{
	m_sqliteBookmarkStorage.removeBookmark(id);
}

void PersistentStorage::removeBookmarkCategory(Id id)
{
	m_sqliteBookmarkStorage.removeBookmarkCategory(id);
}

std::vector<NodeBookmark> PersistentStorage::getAllNodeBookmarks() const
{
	std::unordered_map<Id, StorageBookmarkCategory> bookmarkCategories;
	for (const StorageBookmarkCategory& bookmarkCategory: m_sqliteBookmarkStorage.getAllBookmarkCategories())
	{
		bookmarkCategories[bookmarkCategory.id] = bookmarkCategory;
	}

	std::unordered_map<Id, std::vector<Id>> bookmarkIdToBookmarkedNodeIds;
	for (const StorageBookmarkedNode& bookmarkedNode: m_sqliteBookmarkStorage.getAllBookmarkedNodes())
	{
		bookmarkIdToBookmarkedNodeIds[bookmarkedNode.bookmarkId].push_back(
			m_sqliteIndexStorage.getNodeBySerializedName(bookmarkedNode.serializedNodeName).id);
	}

	std::vector<NodeBookmark> nodeBookmarks;

	for (const StorageBookmark& storageBookmark: m_sqliteBookmarkStorage.getAllBookmarks())
	{
		auto itCategories = bookmarkCategories.find(storageBookmark.categoryId);
		auto itNodeIds = bookmarkIdToBookmarkedNodeIds.find(storageBookmark.id);
		if (itCategories != bookmarkCategories.end() && itNodeIds != bookmarkIdToBookmarkedNodeIds.end())
		{
			NodeBookmark bookmark(
				storageBookmark.id,
				storageBookmark.name,
				storageBookmark.comment,
				storageBookmark.timestamp,
				BookmarkCategory(itCategories->second.id, itCategories->second.name)
			);
			bookmark.setNodeIds(itNodeIds->second);
			bookmark.setIsValid();
			nodeBookmarks.push_back(bookmark);
		}
	}

	return nodeBookmarks;
}

std::vector<EdgeBookmark> PersistentStorage::getAllEdgeBookmarks() const
{
	std::unordered_map<Id, StorageBookmarkCategory> bookmarkCategories;
	for (const StorageBookmarkCategory& bookmarkCategory: m_sqliteBookmarkStorage.getAllBookmarkCategories())
	{
		bookmarkCategories[bookmarkCategory.id] = bookmarkCategory;
	}

	std::unordered_map<Id, std::vector<StorageBookmarkedEdge>> bookmarkIdToBookmarkedEdges;
	for (const StorageBookmarkedEdge& bookmarkedEdge: m_sqliteBookmarkStorage.getAllBookmarkedEdges())
	{
		bookmarkIdToBookmarkedEdges[bookmarkedEdge.bookmarkId].push_back(bookmarkedEdge);
	}

	std::vector<EdgeBookmark> edgeBookmarks;

	Cache<std::string, Id> nodeIdCache([&](std::string serializedNodeName)
		{
			return m_sqliteIndexStorage.getNodeBySerializedName(serializedNodeName).id;
		}
	);

	for (const StorageBookmark& storageBookmark: m_sqliteBookmarkStorage.getAllBookmarks())
	{
		auto itCategories = bookmarkCategories.find(storageBookmark.categoryId);
		auto itBookmarkedEdges = bookmarkIdToBookmarkedEdges.find(storageBookmark.id);
		if (itCategories != bookmarkCategories.end() && itBookmarkedEdges != bookmarkIdToBookmarkedEdges.end())
		{
			EdgeBookmark bookmark(
				storageBookmark.id,
				storageBookmark.name,
				storageBookmark.comment,
				storageBookmark.timestamp,
				BookmarkCategory(itCategories->second.id, itCategories->second.name)
			);

			Id activeNodeId = 0;
			for (const StorageBookmarkedEdge& bookmarkedEdge: itBookmarkedEdges->second)
			{
				const Id sourceNodeId = nodeIdCache.getValue(bookmarkedEdge.serializedSourceNodeName);
				const Id targetNodeId = nodeIdCache.getValue(bookmarkedEdge.serializedTargetNodeName);
				const Id edgeId =
					m_sqliteIndexStorage.getEdgeBySourceTargetType(sourceNodeId, targetNodeId, bookmarkedEdge.edgeType).id;
				bookmark.addEdgeId(edgeId);

				if (activeNodeId == 0)
				{
					activeNodeId = bookmarkedEdge.sourceNodeActive ? sourceNodeId : targetNodeId;
				}
			}
			bookmark.setActiveNodeId(activeNodeId);
			bookmark.setIsValid();
			edgeBookmarks.push_back(bookmark);
		}
	}

	return edgeBookmarks;
}

std::vector<BookmarkCategory> PersistentStorage::getAllBookmarkCategories() const
{
	std::vector<BookmarkCategory> categories;
	for (const StorageBookmarkCategory storageBookmarkCategoriy: m_sqliteBookmarkStorage.getAllBookmarkCategories())
	{
		categories.push_back(BookmarkCategory(storageBookmarkCategoriy.id, storageBookmarkCategoriy.name));
	}
	return categories;
}

TooltipInfo PersistentStorage::getTooltipInfoForTokenIds(const std::vector<Id>& tokenIds, TooltipOrigin origin) const
{
	TRACE();

	TooltipInfo info;

	if (!tokenIds.size())
	{
		return info;
	}

	StorageNode node = m_sqliteIndexStorage.getFirstById<StorageNode>(tokenIds[0]);
	if (node.id == 0 && origin == TOOLTIP_ORIGIN_CODE)
	{
		StorageEdge edge = m_sqliteIndexStorage.getFirstById<StorageEdge>(tokenIds[0]);

		if (edge.id > 0)
		{
			node = m_sqliteIndexStorage.getFirstById<StorageNode>(edge.targetNodeId);
		}
	}

	if (node.id == 0)
	{
		return info;
	}

	Node::NodeType type = Node::intToType(node.type);
	info.title = Node::getReadableTypeString(type);

	DefinitionKind defKind = DEFINITION_NONE;
	StorageSymbol symbol = m_sqliteIndexStorage.getFirstById<StorageSymbol>(node.id);
	if (symbol.id > 0)
	{
		defKind = intToDefinitionKind(symbol.definitionKind);
	}

	if (type & Node::NODE_MEMBER_TYPE)
	{
		StorageComponentAccess access = m_sqliteIndexStorage.getComponentAccessByNodeId(node.id);
		if (access.nodeId != 0)
		{
			info.title = accessKindToString(intToAccessKind(access.type)) + " " + info.title;
		}
	}

	if (type == Node::NODE_FILE)
	{
		bool complete = false;
		auto it = m_fileNodeComplete.find(node.id);
		if (it != m_fileNodeComplete.end())
		{
			complete = it->second;
		}

		if (!complete)
		{
			info.title = "incomplete " + info.title;
		}
	}
	else if (defKind == DEFINITION_NONE && type != Node::NODE_NON_INDEXED)
	{
		info.title = "non-indexed " + info.title;
	}
	else if (defKind == DEFINITION_IMPLICIT)
	{
		info.title = "implicit " + info.title;
	}

	info.count = 0;
	info.countText = "reference";
	for (auto edge : m_sqliteIndexStorage.getEdgesByTargetId(node.id))
	{
		if (Edge::intToType(edge.type) != Edge::EDGE_MEMBER)
		{
			info.count++;
		}
	}

	info.snippets.push_back(getTooltipSnippetForNode(node));

	if (origin == TOOLTIP_ORIGIN_CODE)
	{
		info.offset = Vec2i(20, 30);
	}
	else
	{
		info.offset = Vec2i(50, 20);
	}

	return info;
}

TooltipSnippet PersistentStorage::getTooltipSnippetForNode(const StorageNode& node) const
{
	TRACE();

	TooltipSnippet snippet;
	NameHierarchy nameHierarchy = NameHierarchy::deserialize(node.serializedName);
	snippet.code = nameHierarchy.getQualifiedNameWithSignature();
	snippet.locationFile = std::make_shared<SourceLocationFile>(
		FilePath(nameHierarchy.getDelimiter() == NAME_DELIMITER_JAVA ? "main.java" : "main.cpp"), true, true);

	if (Node::intToType(node.type) & (Node::NODE_FUNCTION | Node::NODE_METHOD | Node::NODE_FIELD | Node::NODE_GLOBAL_VARIABLE))
	{
		snippet.code = utility::breakSignature(
			nameHierarchy.getSignature().getPrefix(),
			nameHierarchy.getQualifiedName(),
			nameHierarchy.getSignature().getPostfix(),
			50,
			ApplicationSettings::getInstance()->getCodeTabWidth()
		);

		std::vector<Id> typeNodeIds;
		for (auto edge : m_sqliteIndexStorage.getEdgesBySourceId(node.id))
		{
			if (Edge::intToType(edge.type) == Edge::EDGE_TYPE_USAGE)
			{
				typeNodeIds.push_back(edge.targetNodeId);
			}
		}

		std::set<std::pair<std::string, Id>, bool(*)(const std::pair<std::string, Id>&, const std::pair<std::string, Id>&)> typeNames(
			[](const std::pair<std::string, Id>& a, const std::pair<std::string, Id>& b)
			{
				if (a.first.size() == b.first.size())
				{
					return a.first < b.first;
				}

				return a.first.size() > b.first.size();
			}
		);

		typeNames.insert(std::make_pair(nameHierarchy.getQualifiedName(), node.id));
		for (auto typeNode : m_sqliteIndexStorage.getAllByIds<StorageNode>(typeNodeIds))
		{
			typeNames.insert(std::make_pair(
				NameHierarchy::deserialize(typeNode.serializedName).getQualifiedName(),
				typeNode.id
			));
		}

		Id locationId = 1;
		std::vector<std::pair<size_t, size_t>> locationRanges;
		for (auto p : typeNames)
		{
			size_t pos = 0;
			while (pos != std::string::npos)
			{
				pos = snippet.code.find(p.first, pos);
				if (pos == std::string::npos)
				{
					continue;
				}

				bool inRange = false;
				for (auto p : locationRanges)
				{
					if (pos + 1 >= p.first && pos + 1 <= p.second)
					{
						inRange = true;
						pos = p.second + 1;
						break;
					}
				}

				if (!inRange)
				{
					snippet.locationFile->addSourceLocation(
						LOCATION_TOKEN, locationId, std::vector<Id>(1, p.second), 1, pos + 1, 1, pos + p.first.size());

					locationRanges.push_back(std::make_pair(pos + 1, pos + p.first.size()));
					pos += p.first.size();
					locationId++;
				}
			}
		}
	}
	else
	{
		snippet.locationFile->addSourceLocation(
			LOCATION_TOKEN, 0, std::vector<Id>(1, node.id), 1, 1, 1, snippet.code.size());
	}

	return snippet;
}

Id PersistentStorage::getFileNodeId(const FilePath& filePath) const
{
	if (filePath.empty())
	{
		LOG_ERROR("No file path set");
		return 0;
	}

	std::map<FilePath, Id>::const_iterator it = m_fileNodeIds.find(filePath);

	if (it != m_fileNodeIds.end())
	{
		return it->second;
	}

	return 0;
}

std::vector<Id> PersistentStorage::getFileNodeIds(const std::vector<FilePath>& filePaths) const
{
	std::vector<Id> ids;
	for (const FilePath& path : filePaths)
	{
		ids.push_back(getFileNodeId(path));
	}
	return ids;
}

std::set<Id> PersistentStorage::getFileNodeIds(const std::set<FilePath>& filePaths) const
{
	std::set<Id> ids;
	for (const FilePath& path : filePaths)
	{
		ids.insert(getFileNodeId(path));
	}
	return ids;
}

FilePath PersistentStorage::getFileNodePath(Id fileId) const
{
	if (fileId == 0)
	{
		LOG_ERROR("No file id set");
		return FilePath();
	}

	std::map<Id, FilePath>::const_iterator it = m_fileNodePaths.find(fileId);

	if (it != m_fileNodePaths.end())
	{
		return it->second;
	}

	return FilePath();
}

bool PersistentStorage::getFileNodeComplete(const FilePath& filePath) const
{
	auto it = m_fileNodeIds.find(filePath);
	if (it != m_fileNodeIds.end())
	{
		auto it2 = m_fileNodeComplete.find(it->second);
		if (it2 != m_fileNodeComplete.end())
		{
			return it2->second;
		}
	}

	return false;
}

std::unordered_map<Id, std::set<Id>> PersistentStorage::getFileIdToIncludingFileIdMap() const
{
	std::unordered_map<Id, std::set<Id>> fileIdToIncludingFileIdMap;
	for (const StorageEdge& includeEdge : m_sqliteIndexStorage.getEdgesByType(Edge::typeToInt(Edge::EDGE_INCLUDE)))
	{
		fileIdToIncludingFileIdMap[includeEdge.targetNodeId].insert(includeEdge.sourceNodeId);
	}
	return fileIdToIncludingFileIdMap;
}

std::unordered_map<Id, std::set<Id>> PersistentStorage::getFileIdToImportingFileIdMap() const
{
	std::unordered_map<Id, std::set<Id>> fileIdToImportingFileIdMap;
	{
		std::vector<Id> importedElementIds;
		std::map<Id, std::set<Id>> elementIdToImportingFileIds;

		for (const StorageEdge& importEdge : m_sqliteIndexStorage.getEdgesByType(Edge::typeToInt(Edge::EDGE_IMPORT)))
		{
			importedElementIds.push_back(importEdge.targetNodeId);
			elementIdToImportingFileIds[importEdge.targetNodeId].insert(importEdge.sourceNodeId);
		}

		std::unordered_map<Id, Id> importedElementIdToFileNodeId;
		{
			std::vector<Id> importedSourceLocationIds;
			std::unordered_map<Id, Id> importedSourceLocationToElementIds;
			for (const StorageOccurrence& occurrence: m_sqliteIndexStorage.getOccurrencesForElementIds(importedElementIds))
			{
				importedSourceLocationIds.push_back(occurrence.sourceLocationId);
				importedSourceLocationToElementIds[occurrence.sourceLocationId] = occurrence.elementId;
			}

			for (const StorageSourceLocation& sourceLocation:
					m_sqliteIndexStorage.getAllByIds<StorageSourceLocation>(importedSourceLocationIds))
			{
				auto it = importedSourceLocationToElementIds.find(sourceLocation.id);
				if (it != importedSourceLocationToElementIds.end())
				{
					importedElementIdToFileNodeId[it->second] = sourceLocation.fileNodeId;
				}
			}
		}

		for (const auto& it: elementIdToImportingFileIds)
		{
			auto importedFileIt = importedElementIdToFileNodeId.find(it.first);
			if (importedFileIt != importedElementIdToFileNodeId.end())
			{
				fileIdToImportingFileIdMap[importedFileIt->second].insert(it.second.begin(), it.second.end());
			}
		}
	}
	return fileIdToImportingFileIdMap;
}

std::set<Id> PersistentStorage::getReferenced(
	const std::set<Id>& ids, std::unordered_map<Id, std::set<Id>> idToReferencingIdMap) const
{
	std::unordered_map<Id, std::set<Id>> idToReferencedIdMap;
	for (auto it: idToReferencingIdMap)
	{
		for (Id referencingId: it.second)
		{
			idToReferencedIdMap[referencingId].insert(it.first);
		}
	}

	return getReferencing(ids, idToReferencedIdMap);
}

std::set<Id> PersistentStorage::getReferencing(
	const std::set<Id>& ids, std::unordered_map<Id, std::set<Id>> idToReferencingIdMap) const
{
	std::set<Id> referencingIds;

	std::set<Id> processingIds = ids;
	std::set<Id> processedIds;

	while (!processingIds.empty())
	{
		std::set<Id> tempIds = processingIds;
		utility::append(processedIds, processingIds);
		processingIds.clear();

		for (Id id: tempIds)
		{
			utility::append(referencingIds, idToReferencingIdMap[id]);
			for (Id referencingId: idToReferencingIdMap[id])
			{
				if (processedIds.find(referencingId) == processedIds.end())
				{
					processingIds.insert(referencingId);
				}
			}
		}
	}

	return referencingIds;

}

std::set<FilePath> PersistentStorage::getReferencedByIncludes(const std::set<FilePath>& filePaths)
{
	std::set<Id> ids = getReferenced(getFileNodeIds(filePaths), getFileIdToIncludingFileIdMap());

	std::set<FilePath> paths;
	for (Id id: ids)
	{ // TODO: performance optimize: use just one request for all ids!
		paths.insert(getFileNodePath(id));
	}

	return paths;
}

std::set<FilePath> PersistentStorage::getReferencedByImports(const std::set<FilePath>& filePaths)
{
	std::set<Id> ids = getReferenced(getFileNodeIds(filePaths), getFileIdToImportingFileIdMap());

	std::set<FilePath> paths;
	for (Id id: ids)
	{
		paths.insert(getFileNodePath(id));
	}

	return paths;
}

std::set<FilePath> PersistentStorage::getReferencingByIncludes(const std::set<FilePath>& filePaths)
{
	std::set<Id> ids = getReferencing(getFileNodeIds(filePaths), getFileIdToIncludingFileIdMap());

	std::set<FilePath> paths;
	for (Id id: ids)
	{
		paths.insert(getFileNodePath(id));
	}

	return paths;
}

std::set<FilePath> PersistentStorage::getReferencingByImports(const std::set<FilePath>& filePaths)
{
	std::set<Id> ids = getReferencing(getFileNodeIds(filePaths), getFileIdToImportingFileIdMap());

	std::set<FilePath> paths;
	for (Id id: ids)
	{
		paths.insert(getFileNodePath(id));
	}

	return paths;
}

void PersistentStorage::addNodesToGraph(const std::vector<Id>& newNodeIds, Graph* graph, bool addChildCount) const
{
	TRACE();

	std::vector<Id> nodeIds;
	if (graph->getNodeCount())
	{
		for (Id id : newNodeIds)
		{
			if (!graph->getNodeById(id))
			{
				nodeIds.push_back(id);
			}
		}
	}
	else
	{
		nodeIds = newNodeIds;
	}

	if (nodeIds.size() == 0)
	{
		return;
	}

	for (const StorageNode& storageNode : m_sqliteIndexStorage.getAllByIds<StorageNode>(nodeIds))
	{
		const Node::NodeType type = Node::intToType(storageNode.type);
		if (type == Node::NODE_FILE)
		{
			const FilePath filePath(NameHierarchy::deserialize(storageNode.serializedName).getRawName());

			bool defined = true;
			auto it = m_fileNodeComplete.find(storageNode.id);
			if (it != m_fileNodeComplete.end())
			{
				defined = it->second;
			}

			Node* node = graph->createNode(
				storageNode.id,
				Node::NODE_FILE,
				NameHierarchy(filePath.fileName(), NAME_DELIMITER_FILE),
				defined
			);
			node->addComponentFilePath(std::make_shared<TokenComponentFilePath>(filePath));
			node->setExplicit(defined);
		}
		else
		{
			const NameHierarchy nameHierarchy = NameHierarchy::deserialize(storageNode.serializedName);

			DefinitionKind defKind = DEFINITION_NONE;
			auto it = m_symbolDefinitionKinds.find(storageNode.id);
			if (it != m_symbolDefinitionKinds.end())
			{
				defKind = it->second;
			}

			Node* node = graph->createNode(
				storageNode.id,
				type,
				nameHierarchy,
				defKind != DEFINITION_NONE
			);

			if (defKind == DEFINITION_IMPLICIT)
			{
				node->setImplicit(true);
			}
			else if (defKind == DEFINITION_EXPLICIT)
			{
				node->setExplicit(true);
			}

			if (addChildCount)
			{
				node->setChildCount(m_hierarchyCache.getFirstNonImplicitChildIdsCountForNodeId(storageNode.id));
			}
		}
	}
}

void PersistentStorage::addEdgesToGraph(const std::vector<Id>& newEdgeIds, Graph* graph) const
{
	TRACE();

	std::vector<Id> edgeIds;
	for (Id id : newEdgeIds)
	{
		if (!graph->getEdgeById(id))
		{
			edgeIds.push_back(id);
		}
	}

	if (edgeIds.size() == 0)
	{
		return;
	}

	for (const StorageEdge& storageEdge : m_sqliteIndexStorage.getAllByIds<StorageEdge>(edgeIds))
	{
		Node* sourceNode = graph->getNodeById(storageEdge.sourceNodeId);
		Node* targetNode = graph->getNodeById(storageEdge.targetNodeId);

		if (sourceNode && targetNode)
		{
			graph->createEdge(storageEdge.id, Edge::intToType(storageEdge.type), sourceNode, targetNode);
		}
		else
		{
			LOG_ERROR("Can't add edge because nodes are not present");
		}
	}
}

void PersistentStorage::addNodesWithParentsAndEdgesToGraph(
	const std::vector<Id>& nodeIds, const std::vector<Id>& edgeIds, Graph* graph
) const
{
	TRACE();

	std::set<Id> allNodeIds(nodeIds.begin(), nodeIds.end());
	std::set<Id> allEdgeIds(edgeIds.begin(), edgeIds.end());

	if (edgeIds.size() > 0)
	{
		for (const StorageEdge& storageEdge : m_sqliteIndexStorage.getAllByIds<StorageEdge>(edgeIds))
		{
			allNodeIds.insert(storageEdge.sourceNodeId);
			allNodeIds.insert(storageEdge.targetNodeId);
		}
	}

	std::set<Id> parentNodeIds;
	for (Id nodeId : allNodeIds)
	{
		m_hierarchyCache.addAllVisibleParentIdsForNodeId(nodeId, &parentNodeIds, &allEdgeIds);
	}

	allNodeIds.insert(parentNodeIds.begin(), parentNodeIds.end());

	addNodesToGraph(utility::toVector(allNodeIds), graph, true);
	addEdgesToGraph(utility::toVector(allEdgeIds), graph);
}

void PersistentStorage::addAggregationEdgesToGraph(
	const Id nodeId, const std::vector<StorageEdge>& edgesToAggregate, Graph* graph) const
{
	TRACE();

	struct EdgeInfo
	{
		Id edgeId;
		bool forward;
	};

	// build aggregation edges:
	// get all children of the active node
	std::set<Id> childNodeIdsSet, edgeIdsSet;
	m_hierarchyCache.addAllChildIdsForNodeId(nodeId, &childNodeIdsSet, &edgeIdsSet);
	std::vector<Id> childNodeIds = utility::toVector(childNodeIdsSet);
	if (childNodeIds.size() == 0 && edgesToAggregate.size() == 0)
	{
		return;
	}

	// get all edges of the children
	std::map<Id, std::vector<EdgeInfo>> connectedNodeIds;
	for (const StorageEdge& edge : edgesToAggregate)
	{
		bool isSource = nodeId == edge.sourceNodeId;
		EdgeInfo edgeInfo;
		edgeInfo.edgeId = edge.id;
		edgeInfo.forward = isSource;
		connectedNodeIds[isSource ? edge.targetNodeId : edge.sourceNodeId].push_back(edgeInfo);
	}

	std::vector<StorageEdge> outgoingEdges = m_sqliteIndexStorage.getEdgesBySourceIds(childNodeIds);
	for (const StorageEdge& outEdge : outgoingEdges)
	{
		EdgeInfo edgeInfo;
		edgeInfo.edgeId = outEdge.id;
		edgeInfo.forward = true;
		connectedNodeIds[outEdge.targetNodeId].push_back(edgeInfo);
	}

	std::vector<StorageEdge> incomingEdges = m_sqliteIndexStorage.getEdgesByTargetIds(childNodeIds);
	for (const StorageEdge& inEdge : incomingEdges)
	{
		EdgeInfo edgeInfo;
		edgeInfo.edgeId = inEdge.id;
		edgeInfo.forward = false;
		connectedNodeIds[inEdge.sourceNodeId].push_back(edgeInfo);
	}

	// get all parent nodes of all connected nodes (up to last level except namespace/undefined)
	Id nodeParentNodeId = m_hierarchyCache.getLastVisibleParentNodeId(nodeId);

	std::map<Id, std::vector<EdgeInfo>> connectedParentNodeIds;
	for (const std::pair<Id, std::vector<EdgeInfo>>& p : connectedNodeIds)
	{
		Id parentNodeId = m_hierarchyCache.getLastVisibleParentNodeId(p.first);

		if (parentNodeId != nodeParentNodeId)
		{
			utility::append(connectedParentNodeIds[parentNodeId], p.second);
		}
	}

	// add hierarchies of these parents
	std::vector<Id> nodeIdsToAdd;
	for (const std::pair<Id, std::vector<EdgeInfo>> p : connectedParentNodeIds)
	{
		const Id aggregationTargetNodeId = p.first;
		if (!graph->getNodeById(aggregationTargetNodeId))
		{
			nodeIdsToAdd.push_back(aggregationTargetNodeId);
		}
	}
	addNodesWithParentsAndEdgesToGraph(nodeIdsToAdd, std::vector<Id>(), graph);

	// create aggregation edges between parents and active node
	Node* sourceNode = graph->getNodeById(nodeId);
	for (const std::pair<Id, std::vector<EdgeInfo>> p : connectedParentNodeIds)
	{
		const Id aggregationTargetNodeId = p.first;

		Node* targetNode = graph->getNodeById(aggregationTargetNodeId);
		if (!targetNode)
		{
			LOG_ERROR("Aggregation target node not present.");
		}

		std::shared_ptr<TokenComponentAggregation> componentAggregation = std::make_shared<TokenComponentAggregation>();
		for (const EdgeInfo& edgeInfo: p.second)
		{
			componentAggregation->addAggregationId(edgeInfo.edgeId, edgeInfo.forward);
		}

		// Set first bit to 1 to avoid collisions
		Id aggregationId = ~(~size_t(0) >> 1) + *componentAggregation->getAggregationIds().begin();

		Edge* edge = graph->createEdge(aggregationId, Edge::EDGE_AGGREGATION, sourceNode, targetNode);
		edge->addComponentAggregation(componentAggregation);
	}
}

void PersistentStorage::addComponentAccessToGraph(Graph* graph) const
{
	TRACE();

	std::vector<Id> nodeIds;
	graph->forEachNode(
		[&nodeIds](Node* node)
		{
			nodeIds.push_back(node->getId());
		}
	);

	std::vector<StorageComponentAccess> accesses = m_sqliteIndexStorage.getComponentAccessesByNodeIds(nodeIds);
	for (const StorageComponentAccess& access : accesses)
	{
		if (access.nodeId != 0)
		{
			graph->getNodeById(access.nodeId)->addComponentAccess(
				std::make_shared<TokenComponentAccess>(intToAccessKind(access.type)));
		}
	}
}

void PersistentStorage::addCompleteFlagsToSourceLocationCollection(SourceLocationCollection* collection) const
{
	TRACE();

	collection->forEachSourceLocationFile(
		[this](std::shared_ptr<SourceLocationFile> file)
		{
			file->setIsComplete(getFileNodeComplete(file->getFilePath()));
		}
	);
}

void PersistentStorage::addInheritanceChainsToGraph(const std::vector<Id>& activeNodeIds, Graph* graph) const
{
	TRACE();

	std::set<Id> activeNodeIdsSet;
	for (Id activeNodeId : activeNodeIds)
	{
		std::set<Id> visibleParentIds, edgeIds;
		visibleParentIds.insert(activeNodeId);
		m_hierarchyCache.addAllVisibleParentIdsForNodeId(activeNodeId, &visibleParentIds, &edgeIds);

		for (Id nodeId : visibleParentIds)
		{
			Node* node = graph->getNodeById(nodeId);
			if (node && node->isType(Node::NODE_INHERITABLE_TYPE))
			{
				activeNodeIdsSet.insert(node->getId());
			}
		}
	}

	std::set<Id> nodeIdsSet;
	graph->forEachNode(
		[&nodeIdsSet, &activeNodeIdsSet](Node* node)
		{
			if (node->isType(Node::NODE_INHERITABLE_TYPE) && activeNodeIdsSet.find(node->getId()) == activeNodeIdsSet.end())
			{
				nodeIdsSet.insert(node->getId());
			}
		}
	);

	std::vector<std::set<Id>*> nodeIdSets;
	nodeIdSets.push_back(&activeNodeIdsSet);
	nodeIdSets.push_back(&nodeIdsSet);

	size_t inheritanceEdgeCount = 1;

	for (size_t i = 0; i < nodeIdSets.size(); i++)
	{
		for (const Id nodeId : *nodeIdSets[i])
		{
			for (const std::tuple<Id, Id, std::vector<Id>>& edge :
				m_hierarchyCache.getInheritanceEdgesForNodeId(nodeId, *nodeIdSets[(i + 1) % 2]))
			{
				Id sourceId = std::get<0>(edge);
				Id targetId = std::get<1>(edge);
				std::vector<Id> edgeIds = std::get<2>(edge);

				if (!edgeIds.size() || (edgeIds.size() == 1 && graph->getEdgeById(edgeIds[0])))
				{
					continue;
				}

				// Set first 2 bits to 1 to avoid collisions
				Id inheritanceEdgeId = ~(~size_t(0) >> 2) + inheritanceEdgeCount++;

				Edge* inheritanceEdge = graph->createEdge(
					inheritanceEdgeId, Edge::EDGE_INHERITANCE, graph->getNodeById(sourceId), graph->getNodeById(targetId));

				inheritanceEdge->addComponentInheritanceChain(std::make_shared<TokenComponentInheritanceChain>(edgeIds));
			}
		}
	}
}

void PersistentStorage::buildFilePathMaps()
{
	TRACE();

	for (StorageFile file: m_sqliteIndexStorage.getAll<StorageFile>())
	{
		m_fileNodeIds.emplace(FilePath(file.filePath), file.id);
		m_fileNodePaths.emplace(file.id, FilePath(file.filePath));
		m_fileNodeComplete.emplace(file.id, file.complete);
	}

	for (StorageSymbol symbol : m_sqliteIndexStorage.getAll<StorageSymbol>())
	{
		m_symbolDefinitionKinds.emplace(symbol.id, intToDefinitionKind(symbol.definitionKind));
	}
}

void PersistentStorage::buildSearchIndex()
{
	TRACE();

	FilePath dbPath = getDbFilePath();

	for (StorageNode node : m_sqliteIndexStorage.getAll<StorageNode>())
	{
		if (Node::intToType(node.type) == Node::NODE_FILE)
		{
			auto it = m_fileNodePaths.find(node.id);
			if (it != m_fileNodePaths.end())
			{
				FilePath filePath(it->second);

				if (filePath.exists())
				{
					filePath = filePath.relativeTo(dbPath);
				}

				m_fileIndex.addNode(node.id, filePath.str());
			}
		}
		else
		{
			auto it = m_symbolDefinitionKinds.find(node.id);
			if (it == m_symbolDefinitionKinds.end() || it->second != DEFINITION_IMPLICIT)
			{
				// we don't use the signature here, so elements with the same signature share the same node.
				m_symbolIndex.addNode(node.id, NameHierarchy::deserialize(node.serializedName).getQualifiedName());
			}
		}
	}

	m_symbolIndex.finishSetup();
	m_fileIndex.finishSetup();
}

void PersistentStorage::buildFullTextSearchIndex() const
{
	TRACE();

	for (StorageFile file : m_sqliteIndexStorage.getAll<StorageFile>())
	{
		m_fullTextSearchIndex.addFile(file.id, m_sqliteIndexStorage.getFileContentById(file.id)->getText());
	}
}

void PersistentStorage::buildHierarchyCache()
{
	TRACE();

	std::vector<StorageEdge> memberEdges = m_sqliteIndexStorage.getEdgesByType(Edge::typeToInt(Edge::EDGE_MEMBER));

	std::vector<Id> sourceNodeIds;
	for (const StorageEdge& edge : memberEdges)
	{
		sourceNodeIds.push_back(edge.sourceNodeId);
	}

	std::vector<StorageNode> sourceNodes = m_sqliteIndexStorage.getAllByIds<StorageNode>(sourceNodeIds);

	std::map<Id, Node::NodeType> sourceNodeTypeMap;
	for (const StorageNode& node : sourceNodes)
	{
		sourceNodeTypeMap.emplace(node.id, Node::intToType(node.type));
	}

	for (const StorageEdge& edge : memberEdges)
	{
		bool sourceIsVisible = !(sourceNodeTypeMap[edge.sourceNodeId] & Node::NODE_NOT_VISIBLE);

		bool targetIsImplicit = false;
		auto it = m_symbolDefinitionKinds.find(edge.targetNodeId);
		if (it != m_symbolDefinitionKinds.end())
		{
			targetIsImplicit = (it->second == DEFINITION_IMPLICIT);
		}

		m_hierarchyCache.createConnection(
			edge.id, edge.sourceNodeId, edge.targetNodeId, sourceIsVisible, targetIsImplicit);
	}

	std::vector<StorageEdge> inheritanceEdges = m_sqliteIndexStorage.getEdgesByType(Edge::typeToInt(Edge::EDGE_INHERITANCE));
	for (const StorageEdge& edge : inheritanceEdges)
	{
		m_hierarchyCache.createInheritance(edge.id, edge.sourceNodeId, edge.targetNodeId);
	}
}