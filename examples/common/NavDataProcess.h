#pragma once
#include "PVRAssets/PVRAssets.h"
#include <deque>

/***Road types - colour uniforms***/
const glm::vec4 ClearColor(.8, 0.8, 0.8, 1.f);
const glm::vec4 RoadAreaColourUniform(0.690, 0.769, 0.871, 1.0f);
const glm::vec4 MotorwayColour(1.000, 0.627, 0.478, 1.0f);
const glm::vec4 TrunkRoadColour(0.9725f, 0.6980f, 0.6117f, 1.0f);
const glm::vec4 PrimaryRoadColour(0.9882f, 0.8392f, 0.6431f, 1.0f);
const glm::vec4 SecondaryRoadColour(1.0f, 1.0f, 0.5019f, 1.0f);
const glm::vec4 ServiceRoadColour(0.996f, 0.996f, 0.996f, 1.0f);
const glm::vec4 OtherRoadColour(0.996f, 0.996f, 0.996f, 1.0f);
/**************************/
const glm::vec4 ParkingColourUniform(0.9412f, 0.902f, 0.549f, 1.0f);
const glm::vec4 BuildColourUniform(0.65, 0.65, 0.65, 1.f);
const glm::vec4 OutlineColourUniform(0.4392f, 0.5412f, 0.5647f, 1.0f);

// Describes the different types of way, used for tiling.
namespace WayTypes {
enum WayTypes
{
	Road,
	Parking,
	Building,
	Inner,
	PolygonOutline,
	AreaOutline,
	Default,
};
}

// Describes the different types of road used by the system.
namespace RoadTypes {
enum RoadTypes
{
	Motorway,
	Trunk,
	Primary,
	Secondary,
	Other,
	Service,
	None
};
}

namespace BuildingType {
enum BuildingType
{
	Shop,
	Bar,
	Cafe,
	FastFood,
	Pub,
	College,
	Library,
	University,
	ATM,
	Bank,
	Restaurant,
	Doctors,
	Dentist,
	Hospital,
	Pharmacy,
	Cinema,
	Casino,
	Theatre,
	FireStation,
	Courthouse,
	Police,
	PostOffice,
	Toilets,
	PlaceOfWorship,
	PetrolStation,
	Parking,
	Other,
	PostBox,
	Veterinary,
	Embassy,
	HairDresser,
	Butcher,
	Optician,
	Florist,
	None //'None' needs to be last.
};
}

// Describes the sides of a 2D bounding box.
namespace Sides {
enum Sides
{
	Left,
	Top,
	Right,
	Bottom,
	NoSide,
};
}
namespace LOD {
enum Levels
{
	L0,
	L1,
	L2,
	L3,
	L4,
	L5,
	L6,
	Count,
	LabelLOD = L4,
	IconLOD = L3,
	AmenityLabelLOD = L3
};
}

// Stores the minimum and maximum latitude & longitude of the map.
struct Bounds
{
	glm::dvec2 min;
	glm::dvec2 max;
};

// Stores a key-value pair.
struct Tag
{
	std::string key;
	std::string value;
};

struct IntersectionData
{
	std::vector<uint64_t> nodes;
	std::vector<std::pair<uint32_t, glm::uvec2> > junctionWays;
	bool isBound;
};

struct BoundaryData
{
	bool consumed;
	uint32_t index;
};

/* A struct which stores node data.
Id - Unique node identification number,
index - Used for index drawing of nodes,
coords - Location in 2D space,
texCoords - Texture co-ordinates for AA lines,
wayIds - Ids of ways that this node belongs to,
tileBoundNode - Is this node on a tile boundary. */
struct Vertex
{
	uint64_t id;
	uint32_t index;
	glm::dvec2 coords;
	double height;
	glm::vec2 texCoords;
	std::vector<uint64_t> wayIds;
	bool tileBoundNode;

	Vertex(uint64_t userId = 0, glm::dvec2 userCoords = glm::dvec2(0, 0), bool userTileBoundNode = false,
		glm::vec2 userTexCoords = glm::vec2(
#ifdef NAV_3D
			1, 1
#else
			-10000.f, -10000.f
#endif
			))
	{
		id = userId;
		coords = userCoords;
		tileBoundNode = userTileBoundNode;
		texCoords = userTexCoords;
		index = 0;
		height = 0;
	}

	Vertex& operator=(Vertex&& rhs)
	{
		id = rhs.id;
		index = rhs.index;
		coords = rhs.coords;
		texCoords = rhs.texCoords;
		wayIds = std::move(rhs.wayIds);
		height = rhs.height;
		tileBoundNode = rhs.tileBoundNode;
		return *this;
	}
	Vertex(Vertex&& rhs)
		: id(rhs.id), index(rhs.index), coords(rhs.coords), height(rhs.height), texCoords(rhs.texCoords), wayIds(std::move(rhs.wayIds)), tileBoundNode(rhs.tileBoundNode)
	{}

	Vertex& operator=(const Vertex& rhs)
	{
		id = rhs.id;
		index = rhs.index;
		coords = rhs.coords;
		texCoords = rhs.texCoords;
		wayIds = rhs.wayIds;
		height = rhs.height;
		tileBoundNode = rhs.tileBoundNode;
		return *this;
	}
	Vertex(const Vertex& rhs) : id(rhs.id), index(rhs.index), coords(rhs.coords), height(rhs.height), texCoords(rhs.texCoords), wayIds(rhs.wayIds), tileBoundNode(rhs.tileBoundNode)
	{}
};

struct LabelData
{
	std::string name;
	glm::dvec2 coords;
	float rotation;
	float scale;
	uint64_t id;
	bool isAmenityLabel;
	LOD::Levels maxLodLevel;
	float distToBoundary;
	float distToEndOfSegment;
};

struct IconData
{
	BuildingType::BuildingType buildingType;
	glm::dvec2 coords;
	float scale;
	LOD::Levels lodLevel;
	uint64_t id;
};

struct AmenityLabelData : public LabelData
{
	IconData iconData;
};

struct RouteData
{
	glm::dvec2 point;
	float distanceToNext;
	double rotation;
	glm::vec2 dir;
	std::string name;
	RouteData() : point(glm::dvec2(0.0)), distanceToNext(0.0f), rotation(0.0f) {}
};

// Element: Ordered series of nodes used to represent linear features or
// boundaries
class Way
{
public:
	uint64_t id;
	std::vector<uint64_t> nodeIds;
	double width;
	bool area;
	bool inner;
	bool tileBoundWay;
	bool isIntersection;
	bool isRoundabout;
	bool isFork;
	std::vector<Tag> tags;
	RoadTypes::RoadTypes roadType;
};

// This is used to store road ways that have been converted into triangles
class ConvertedWay : public Way
{
public:
	std::vector<std::array<uint64_t, 3> > triangulatedIds;

	ConvertedWay(uint64_t userId = 0, bool userArea = false, const std::vector<Tag>& userTags = std::vector<Tag>{}, RoadTypes::RoadTypes type = RoadTypes::None,
		double roadWidth = 0, bool intersection = false, bool roundabout = false, bool fork = false)
	{
		id = userId;
		area = userArea;
		tags = userTags;
		roadType = type;
		width = roadWidth;
		isIntersection = intersection;
		isRoundabout = roundabout;
		isFork = fork;
	}
};

// Structure for storing data for an individual map tile.
struct Tile
{
	glm::dvec2 min;
	glm::dvec2 max;
	glm::vec2 screenMin;
	glm::vec2 screenMax;

	std::map<uint64_t, Vertex> nodes;
	std::vector<Way> areaWays;
	std::vector<Way> roadWays;
	std::vector<Way> parkingWays;
	std::vector<Way> buildWays;
	std::vector<Way> innerWays;
	std::vector<LabelData> labels[LOD::Count];
	std::vector<IconData> icons[LOD::Count];
	std::vector<AmenityLabelData> amenityLabels[LOD::Count];
	std::vector<uint64_t> areaOutlineIds;
	std::vector<uint64_t> polygonOutlineIds;

	struct VertexData
	{
		glm::vec3 pos;
		glm::vec2 texCoord;
		glm::vec3 normal;

		VertexData(glm::vec3 position, glm::vec2 textureCoord = glm::vec2(1.0f), glm::vec3 norm = glm::vec3(0.0f)) : pos(position), texCoord(textureCoord), normal(norm) {}
	};

	std::vector<VertexData> vertices;
	std::vector<uint32_t> indices;
};

// Structure for storing data from OSM file.
struct OSM
{
	double lonTileScale;
	double latTileScale;
	uint32_t numCols;
	uint32_t numRows;
	glm::dvec2 minLonLat;
	glm::dvec2 maxLonLat;

	Bounds bounds;
	std::map<uint64_t, Vertex> nodes;
	std::vector<uint64_t> original_intersections;
	std::vector<std::vector<std::map<uint64_t, BoundaryData> > > boundaryNodes;
	std::map<uint64_t, IntersectionData> intersectionNodes;
	std::vector<LabelData> labels[LOD::Count];
	std::vector<AmenityLabelData> amenityLabels[LOD::Count];
	std::vector<IconData> icons[LOD::Count];
	std::set<std::string> uniqueIconNames;

	std::map<uint64_t, Way> originalRoadWays; // roadWays;
	std::map<uint64_t, ConvertedWay> convertedRoads;
	std::map<uint64_t, Way> parkingWays;
	std::map<uint64_t, Way> buildWays;
	std::map<uint64_t, Way> triangulatedRoads; // tempRoads;
	std::vector<uint64_t> areaOutlines;

	std::vector<RouteData> route;

	std::vector<std::vector<Tile> > tiles;

	Way& getOriginalRoadWay(uint64_t wayId)
	{
		return originalRoadWays.find(wayId)->second;
	}
	const Way& getOriginalRoadWay(uint64_t wayId) const
	{
		return originalRoadWays.find(wayId)->second;
	}
	Way& getTriangulatedRoadWay(uint64_t wayId)
	{
		return triangulatedRoads.find(wayId)->second;
	}
	const Way& getTriangulatedRoadWay(uint64_t wayId) const
	{
		return triangulatedRoads.find(wayId)->second;
	}
	const Vertex& getNodeById(uint64_t nodeId) const
	{
		return nodes.find(nodeId)->second;
	}
	Vertex& getNodeById(uint64_t nodeId)
	{
		return nodes.find(nodeId)->second;
	}
	Tile& getTile(uint32_t x, uint32_t y)
	{
		return tiles[x][y];
	}
	Tile& getTile(glm::uvec2 tileCoords)
	{
		return tiles[tileCoords.x][tileCoords.y];
	}
	Vertex& insertOrOverwriteNode(Vertex&& node)
	{
		return (nodes[node.id] = std::move(node));
	}
	Vertex& createNode(uint64_t id)
	{
		Vertex& node = nodes[id];
		node.id = id;
		return node;
	}
	void cleanData();
};

// Remap an old co-ordinate into a new coordinate system.
template<typename T>
inline T remap(const T valueToRemap, const T oldmin, const T oldmax, const T newmin, const T newmax)
{
	return ((valueToRemap - oldmin) / (oldmax - oldmin)) * (newmax - newmin) + newmin;
}

typedef uint64_t NodeId;
typedef glm::dvec2 Vec2;
typedef double Real;

const double boundaryBufferX = 0.05;
const double boundaryBufferY = 0.05;

// Calcuate the rotate time in millisec
// inline float cameraRotationTimeInMs(float angleDeg, float speed)
//{
//	return glm::abs(angleDeg / speed) * 1000;
//}

inline float cameraRotationTimeInMs(float angleDeg, float ms360)
{
	return glm::abs(angleDeg / 360.f * ms360);
}

/*!*****************************************************************************
Class NavDataProcess This class handles the loading of OSM data from an XML file
and pre-processing (i.e. triangulation) the raw data into usable rendering data.
********************************************************************************/
class NavDataProcess
{
public:
	struct RoadParams
	{
		WayTypes::WayTypes wayType;
		uint64_t wayId;
		std::vector<Tag> wayTags;
		bool area;
		RoadTypes::RoadTypes roadType;
		double width;
		bool isIntersection;
		bool isRoundabout;
	};

	// Constructor takes a stream which the class uses to read the XML file.
	NavDataProcess(std::unique_ptr<pvr::Stream> stream, const glm::ivec2& screenDimensions)
	{
		_assetStream = std::move(stream);
		_windowsDim = screenDimensions;
	}

	void clipRoad(const Vertex& vertex0, const Vertex& vertex1, const Vertex& vertex2, const glm::uvec2& minTileIndex, const glm::uvec2& maxTileIndex, const RoadParams& roadParams);

	void clipRoad(const Vertex& vertex0, const Vertex& vertex1, const Vertex& vertex2, uint64_t wayId, const std::vector<Tag>& wayTags, WayTypes::WayTypes wayType, bool area,
		RoadTypes::RoadTypes roadType, double roadWidth, bool isIntersection, bool isRoundabout);

	void recurseClipRoad(const Vertex& vertex0, const Vertex& vertex1, const Vertex& vertex2, const glm::uvec2& minTileIndex, const glm::uvec2& maxTileIndex,
		const RoadParams& roadParams, bool isPlaneVertical);

	// These functions should be called before accessing the tile data to make
	// sure the tiles have been initialised.
	pvr::Result loadAndProcessData();
	void initTiles(); // Call after window width / height is known

	// Public accessor function to tiles.
	void clipAgainst(const Vertex& vertex0, const Vertex& vertex1, const Vertex& vertex2, glm::vec2 planeOrigin, const glm::vec2& planeNorm, Vertex* triFront, Vertex* triBack,
		uint32_t& numTriFront, uint32_t& numTriBack);

	std::vector<std::vector<Tile> >& getTiles()
	{
		return _osm.tiles;
	}
	uint32_t getNumRows() const
	{
		return _osm.numRows;
	}
	uint32_t getNumCols() const
	{
		return _osm.numCols;
	}
	std::vector<RouteData>& getRouteData()
	{
		return _osm.route;
	}
	glm::dvec2 getBoundsMin() const
	{
		return _osm.bounds.min;
	}
	glm::dvec2 getBoundsMax() const
	{
		return _osm.bounds.max;
	}
	const OSM& getOsm() const
	{
		return _osm;
	}
	void processLabelBoundary(LabelData& label, glm::uvec2& tileCoords);

	/*!*********************************************************************************************************************
	\brief  Converts pre-computed route into the appropriate co-ordinate space and
	calculates the routes total true distance
	and partial distances between each node which is used later to animate the
	route.
	***********************************************************************************************************************/
	void convertRoute(const glm::dvec2& mapWorldDim, uint32_t numCols, uint32_t numRows, float& totalRouteDistance);

private:
	// OSM data object.
	OSM _osm;
	glm::ivec2 _windowsDim;
	std::unique_ptr<pvr::Stream> _assetStream;

	// Raw data handling fuctions
	pvr::Result loadOSMData();
	glm::dvec2 lonLatToMetres(const glm::dvec2 origin, const glm::dvec2 point) const;
	void generateIcon(const uint64_t* nodeIds, size_t numNodeIds, const Tag* tags, size_t numTags, uint64_t id);
	void processLabels(const glm::dvec2& mapWorldDim);
	void cleanData();
	void calculateRoute();

	// General utility functions
	std::string getAttributeRef(const std::vector<Tag>& tags) const;
	BuildingType::BuildingType getBuildingType(const Tag* tags, uint32_t numTags) const;
	RoadTypes::RoadTypes getIntersectionRoadType(const std::vector<Way>& ways) const;
	std::string getIntersectionRoadName(const std::vector<std::vector<Tag> >& tags) const;
	double getRoadWidth(const std::vector<Tag>& tags, RoadTypes::RoadTypes& outType) const;
	bool isRoadRoundabout(const std::vector<Tag>& tags) const;
	bool isRoadOneWay(const std::vector<Tag>& tags) const;
	void cleanString(std::string& s) const;

	// Linear mathematics functions

	/*!*********************************************************************************************************************
			\return Return array of 2 new points. Left points (with respect to way
	   direction) is given first.
			\param  firstPoint  First point to use.
			\param  secPoint  Second point to use.
			\param  width   Desired width between the new nodes.
			\param  pointNum  Point to return values for (1, 2).
			\brief  Provides two points on the perpendicular line at distance
	   "width" apart.
			***********************************************************************************************************************/
	std::array<glm::dvec2, 2> findPerpendicularPoints(const glm::dvec2 firstPoint, const glm::dvec2 secPoint, const double width, const int pointNum) const;

	/*!*********************************************************************************************************************
	\return Return array of 2 new points. Left points (with respect to way
	direction) is given first.
	\param  firstPoint  First point to use.
	\param  secPoint  Second point to use.
	\param  thirdPoint  Third point to use.
	\param  width   Desired width between the new nodes.
	\brief  Provides two points on the perpendicular line at distance "width"
	apart for the middle point.
	***********************************************************************************************************************/
	std::array<glm::dvec2, 2> findPerpendicularPoints(const glm::dvec2 firstPoint, const glm::dvec2 secPoint, const glm::dvec2 thirdPoint, const double width) const;

	std::array<glm::dvec2, 2> circleIntersects(const glm::vec2 centre, const double r, const double m, const double constant) const;
	glm::dvec2 lineIntersect(const glm::dvec2 p1, const glm::dvec2 d1, const glm::dvec2 p2, const glm::dvec2 d2) const;
	glm::dvec3 findIntersect(const glm::dvec2 minBounds, const glm::dvec2 maxBounds, const glm::dvec2 inPoint, const glm::dvec2 outPoint) const;
	glm::dvec2 calculateMidPoint(glm::dvec2 p1, glm::dvec2 p2, glm::dvec2 p3) const;

	// Road triangulation functions
	void triangulateAllRoads();
	void calculateIntersections();
	void convertToTriangleList();
	std::vector<uint64_t> triangulateRoad(const std::vector<uint64_t>& nodeIds, const double width);
	std::vector<uint64_t> tessellate(const std::vector<uint64_t>& nodeIds, uint32_t& index, bool splitWay = false);
	std::vector<uint64_t> tessellate(const std::vector<uint64_t>& oldNodeIDs, Real width);
	// Polygon triangulation functions

	/*!*********************************************************************************************************************
			\return std::vector<std::vector<uint64_t>> A vector of triangle ways.
			\param  nodeIds A vector of node IDs for a closed way (anti-clockwise
	   wound).
			\brief  Triangluates an anti-clockwise wound closed way.
			***********************************************************************************************************************/
	void triangulate(std::vector<uint64_t>& nodeIds, std::vector<std::array<uint64_t, 3> >& outTriangulates) const;

	pvr::PolygonWindingOrder checkWinding(const std::vector<uint64_t>& nodeIds) const;
	pvr::PolygonWindingOrder checkWinding(const std::vector<glm::dvec2>& points) const;
	double calculateTriangleArea(const std::vector<glm::dvec2>& points) const;

	// Map tiling functions
	void initialiseTiles();
	void sortTiles();
	void fillTiles(Vertex startNode, Vertex endNode, const uint64_t wayId, const std::vector<Tag> wayTags, const WayTypes::WayTypes wayType, double height = 0,
		const bool addEnd = false, const bool area = false, RoadTypes::RoadTypes type = RoadTypes::None, double width = 0.0, bool isIntersection = false, bool isRoundabout = false,
		bool isFork = false);

	/*!*********************************************************************************************************************
	\brief	Fill tiles with label data.
	***********************************************************************************************************************/
	void fillLabelTiles(LabelData& label, uint32_t lod);

	/*!*********************************************************************************************************************
	\brief	Fill tiles with icon data.
	***********************************************************************************************************************/
	void fillIconTiles(IconData& icon, uint32_t lod);

	/*!*********************************************************************************************************************
	\brief  Fill tiles with Amenity label data.
	***********************************************************************************************************************/
	void fillAmenityTiles(AmenityLabelData& label, uint32_t lod);

	/*!*********************************************************************************************************************
	\param	insertIn	 The vector of ways in which to insert.
	\param	way			 The way to insert.
	\brief	Insert a way (or a node ID) into a given array of ways.
	***********************************************************************************************************************/
	void insertWay(std::vector<Way>& insertIn, Way& way);
	void insert(const glm::uvec2& tileCoords, WayTypes::WayTypes type, Way* w = nullptr, uint64_t id = 0);

	glm::ivec2 findTile(const glm::dvec2& point) const;
	glm::ivec2 findTile2(glm::dvec2& point) const;
	bool isOutOfBounds(const glm::dvec2& point) const;
	bool isTooCloseToBoundary(const glm::dvec2& point) const;
	bool findMapIntersect(glm::dvec2& point1, glm::dvec2& point2) const;
	void addCornerPoints(Tile& tile, std::vector<Way>& way);

	// Texture co-ordinate calculation functions
	void calculateMapBoundaryTexCoords();
	void calculateJunctionTexCoords();
	void calculateCrossRoadJunctionTexCoords(std::vector<std::pair<Vertex*, Vertex*> >& foundNodes, std::vector<std::pair<glm::uvec2, Way*> >& junctionWays);
	uint32_t calculateRoundaboutTexCoordIndices(std::map<uint64_t, Way*>& foundWays, std::vector<std::pair<Vertex*, Vertex*> >& foundNodes);
	uint32_t calculateT_JunctionTexCoordIndices(std::map<uint64_t, Way*>& foundWays, std::vector<std::pair<Vertex*, Vertex*> >& foundNodes, Way* way);
	std::array<uint64_t, 2> calculateEndCaps(Vertex& first, Vertex& second, double width);

	// Comparator functions
	template<typename T>
	inline static bool compareReal(const T a, const T b)
	{
		return glm::abs(a - b) < std::numeric_limits<T>::epsilon();
	}
	inline static bool compareX(const Vertex* const a, const Vertex* const b)
	{
		return a->coords.x < b->coords.x;
	}
	inline static bool compareY(const Vertex* const a, const Vertex* const b)
	{
		return a->coords.y < b->coords.y;
	}
	inline static bool compareRoadTypes(const Way& a, const Way& b)
	{
		return (int(a.roadType) < int(b.roadType));
	}
	inline static bool compareID(const Vertex* const a, const Vertex* const b)
	{
		return a->id == b->id;
	}
};

static const float epsilon = 0.00001f;

// Comparator functions
template<typename T>
inline bool isRealEqual(const T a, const T b)
{
	return glm::abs(a - b) < epsilon;
}
// Comparator functions
template<typename Vec2>
inline bool isVectorEqual(Vec2 a, Vec2 b)
{
	return isRealEqual(a.x, b.x) && isRealEqual(a.y, b.y);
}

inline bool compareX(const Vertex* const a, const Vertex* const b)
{
	return a->coords.x < b->coords.x;
}
inline bool compareY(const Vertex* const a, const Vertex* const b)
{
	return a->coords.y < b->coords.y;
}
inline bool compareRoadTypes(const Way& a, const Way& b)
{
	return (int(a.roadType) < int(b.roadType));
}
inline bool compareID(const Vertex* const a, const Vertex* const b)
{
	return a->id == b->id;
}

/*!*********************************************************************************************************************
\return Find intersection point of 2 lines.
\param  p1  Point on line 1.
\param  d1  Direction vector of line 1.
\param  p2  Point on line 2.
\param  d2  Direction vector of line 2.
\brief  Find intersection point of 2 lines (assumes they intersect).
***********************************************************************************************************************/
template<typename Vec2>
inline bool rayIntersect(const Vec2& p0, const Vec2& d0, const Vec2& p1, const Vec2& d1, typename Vec2::value_type& outDistanceD0, Vec2& outIntersectionPoint)
{
	typedef glm::detail::tvec3<typename Vec2::value_type, glm::precision::defaultp> Vec3;
	outIntersectionPoint = p0;
	outDistanceD0 = 0;
	if (isVectorEqual(p0, p1))
	{
		return true;
	}
	bool retval = pvr::math::intersectLinePlane(p0, d0, p1, -pvr::math::getPerpendicular(d1), outDistanceD0, epsilon);
	if (retval)
	{
		outIntersectionPoint = p0 + d0 * outDistanceD0;
	}
	else if (glm::length(glm::cross(Vec3(p0 - p1, 0), Vec3(d0, 0))) < epsilon) // COINCIDENT!
	{
		outDistanceD0 = 0.5;
		outIntersectionPoint = (p0 + p1) * .5;
		retval = true;
	}
	return retval;
}

/*!*********************************************************************************************************************
\return Find intersection point of 2 lines.
\param  p1  Point on line 1.
\param  d1  Direction vector of line 1.
\param  p2  Point on line 2.
\param  d2  Direction vector of line 2.
\brief  Find intersection point of 2 lines (assumes they intersect).
***********************************************************************************************************************/
template<typename Vec2>
inline bool rayIntersect(const Vec2& p0, const Vec2& d0, const Vec2& p1, const Vec2& d1, Vec2& outIntersectionPoint)
{
	typename Vec2::value_type dummy;
	return rayIntersect(p0, d0, p1, d1, dummy, outIntersectionPoint);
}

inline float distanceToPlane(const glm::vec2 pointToCheck, float planeDist, glm::vec2 planeNorm)
{
	return glm::dot(glm::vec2(planeNorm), pointToCheck) - planeDist;
}

inline float distanceToPlane(const glm::vec2 pointToCheck, glm::vec2 anyPlanePoint, glm::vec2 planeNorm)
{
	return glm::dot(glm::vec2(planeNorm), pointToCheck) - glm::dot(anyPlanePoint, planeNorm);
}

template<typename Vec2>
inline typename Vec2::value_type vectorAngleSine(const Vec2& d0, const Vec2& d1)
{
	using namespace glm;
	return glm::length(glm::cross(normalize(detail::tvec3<typename Vec2::value_type, glm::precision::defaultp>(d0, 0.)),
		normalize(glm::detail::tvec3<typename Vec2::value_type, glm::precision::defaultp>(d1, 0.))));
}

template<typename Vec2>
inline typename Vec2::value_type vectorAngleCosine(const Vec2& d0, const Vec2& d1)
{
	using namespace glm;
	return dot(normalize(d0), normalize(d1));
}
template<typename Vec2>
inline typename Vec2::value_type vectorAngleSine(const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3)
{
	return vectorAngleSine(p1 - p0, p3 - p2);
}

template<typename Vec2>
inline typename Vec2::value_type vectorAngleCosine(const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3)
{
	return vectorAngleCosine(p1 - p0, p3 - p2);
}

template<typename Vec2>
inline typename Vec2::value_type vectorAngleCosine(const Vec2& center, const Vec2& point0, const Vec2& point1)
{
	return vectorAngleCosine(point0 - center, point1 - center);
}

template<typename Vec2>
inline typename Vec2::value_type vectorAngleSine(const Vec2& center, const Vec2& point0, const Vec2& point1)
{
	return vectorAngleSine(point0 - center, point1 - center);
}

/*!*********************************************************************************************************************
\param  s String to sanitise.
\brief  Sanitises the incoming std::string.
***********************************************************************************************************************/
inline void cleanString(std::string& s)
{
	// Remove HTML escape character for '&'
	std::size_t pos;
	pos = s.find("&amp;", 0);
	while (pos != s.npos)
	{
		std::string sub1 = s.substr(0, pos);
		sub1.append(" & ");
		std::string sub2 = s.substr(pos + 5, s.length() - 1);
		s.clear();
		s.append(sub1);
		s.append(sub2);
		pos = s.find("&amp;", 0);
	}

	// Remove HTML escape character for quotation marks.
	pos = s.find("&quot;", 0);
	while (pos != s.npos)
	{
		std::string sub1 = s.substr(0, pos);
		sub1.append(" ");
		std::string sub2 = s.substr(pos + 6, s.length() - 1);
		s.clear();
		s.append(sub1);
		s.append(sub2);
		pos = s.find("&quot;", 0);
	}
}

/*!*********************************************************************************************************************
\return The width of the road.
\type The RoadType enum describing the type of the road
\brief  Use the type of a road to determine its width.
***********************************************************************************************************************/
inline double getRoadWidth(RoadTypes::RoadTypes type)
{
	static const float roadWidths[] = { .015f, .014f, .013f, .012f, .010f, .008f };
	return roadWidths[type];
}

/*!*********************************************************************************************************************
\return bool True if the road is a roundabout otherwise false.
\param  tags Tags for the road.
\brief  Clears data no longer needed from the osm object.
***********************************************************************************************************************/
inline bool isRoadRoundabout(const std::vector<Tag>& tags)
{
	for (auto& tag : tags)
	{
		if (tag.key.compare("junction") == 0 && tag.value.compare("roundabout") == 0)
		{
			return true;
		}
	}
	return false;
}

/*!*********************************************************************************************************************
\return std::string  The unique ID to return.
\param  tags  A collection of tags to search.
\brief  Use the tags to find the unique ID.
***********************************************************************************************************************/
inline const std::string& getAttributeRef(const std::vector<Tag>& tags)
{
	static std::string value;
	for (auto& tag : tags)
	{
		if (tag.key.compare("ref") == 0)
		{
			value = tag.value;
			break;
		}
	}

	return value;
}

/*!*********************************************************************************************************************
\return std::string  Name of the road or empty std::string if no name is
available.
\param  tags  A collection of tags to search.
\brief  Use the tags of a road to determine its name.
***********************************************************************************************************************/
inline std::string getAttributeName(const Tag* tags, size_t numTags)
{
	std::string value;
	for (size_t i = 0; i < numTags; ++i)
	{
		const auto& tag = tags[i];
		if (tag.key.compare("name") == 0)
		{
			value = tag.value;
			break;
		}
	}
	cleanString(value);
	return value;
}

/*!*********************************************************************************************************************
\return std::string the road name for the intersection.
\param  std::vector<std::vector<Tag>> Reference to a vector of vectors which
holds the tags for each way that makes up an intersection.
\brief  Finds the dominant road name for a given intersection.
***********************************************************************************************************************/
inline const std::string& getIntersectionRoadName(const std::vector<std::pair<Tag*, size_t> >& tags)
{
	std::map<std::string, uint32_t> nameCount;
	uint32_t currentCount = 0;
	static std::string name = "";

	for (auto& t : tags)
	{
		std::string name = getAttributeName(t.first, t.second);
		if (!name.empty())
		{
			nameCount[name]++;
		}
	}

	for (auto it = nameCount.begin(); it != nameCount.end(); ++it)
	{
		if (it->second > currentCount)
		{
			name = it->first;
			currentCount = it->second;
		}
	}
	cleanString(name);
	return name;
}

/*!*********************************************************************************************************************
\return RoadTypes the type of road.
\param  ways Reference to a vector of ways which make up the intersection.
\brief  Finds the dominant road type for a given intersection (used to colour
the intersection based on the road type).
***********************************************************************************************************************/
inline RoadTypes::RoadTypes getIntersectionRoadType(const std::vector<Way>& ways)
{
	std::vector<Way> tempWays = ways;
	std::sort(tempWays.begin(), tempWays.end(), compareRoadTypes);

	RoadTypes::RoadTypes current = RoadTypes::Service;

	// Iterate through way and find which road type occurs the most.
	for (auto& way : tempWays)
	{
		current = std::min(current, way.roadType);
	}
	return current;
}

/*!*********************************************************************************************************************
\return bool True if the road is a roundabout otherwise false.
\param  tags Tags for the road.
\brief  Clears data no longer needed from the osm object.
***********************************************************************************************************************/
inline bool isRoadOneWay(const std::vector<Tag>& tags)
{
	for (auto& tag : tags)
	{
		if (tag.key.compare("oneway") == 0 && tag.value.compare("yes") == 0)
		{
			return true;
		}
	}
	return false;
}

inline bool initializeBuildingTypesMap(std::map<std::string, BuildingType::BuildingType>& strings)
{
	strings[""] = BuildingType::None;
	strings["supermarket"] = BuildingType::Shop;
	strings["convenience"] = BuildingType::Shop;
	strings["bar"] = BuildingType::Bar;
	strings["cafe"] = BuildingType::Cafe;
	strings["fast_food"] = BuildingType::FastFood;
	strings["pub"] = BuildingType::Pub;
	strings["college"] = BuildingType::College;
	strings["library"] = BuildingType::Library;
	strings["university"] = BuildingType::University;
	strings["atm"] = BuildingType::ATM;
	strings["bank"] = BuildingType::Bank;
	strings["restaurant"] = BuildingType::Restaurant;
	strings["doctors"] = BuildingType::Doctors;
	strings["dentist"] = BuildingType::Dentist;
	strings["hospital"] = BuildingType::Hospital;
	strings["pharmacy"] = BuildingType::Pharmacy;
	strings["cinema"] = BuildingType::Cinema;
	strings["casino"] = BuildingType::Casino;
	strings["theatre"] = BuildingType::Theatre;
	strings["fire_station"] = BuildingType::FireStation;
	strings["courthouse"] = BuildingType::Courthouse;
	strings["police"] = BuildingType::Police;
	strings["post_office"] = BuildingType::PostOffice;
	strings["toilets"] = BuildingType::Toilets;
	strings["place_of_worship"] = BuildingType::PlaceOfWorship;
	strings["fuel"] = BuildingType::PetrolStation;
	strings["parking"] = BuildingType::Parking;
	strings["post_box"] = BuildingType::PostBox;
	strings["veterinary"] = BuildingType::Veterinary;
	strings["pet"] = BuildingType::Veterinary;
	strings["embassy"] = BuildingType::Embassy;
	strings["hairdresser"] = BuildingType::HairDresser;
	strings["butcher"] = BuildingType::Butcher;
	strings["florist"] = BuildingType::Florist;
	strings["optician"] = BuildingType::Optician;
	return true;
}

inline BuildingType::BuildingType getBuildingType(Tag* tags, size_t numTags)
{
	static std::map<std::string, BuildingType::BuildingType> strings;
	static bool used_to_initialize = initializeBuildingTypesMap(strings);
	for (size_t i = 0; i < numTags; ++i)
	{
		auto& tag = tags[i];
		if (tag.key.compare("amenity") == 0 || tag.key.compare("shop") == 0)
		{
			auto it = strings.find(tag.value);
			if (it != strings.end())
			{
				return it->second;
			}
			break;
		}
	}
	return BuildingType::Other;
}

/*!*********************************************************************************************************************
\param  tile The tile to generate indices for.
\param  way The vector of ways to generate indices for.
\return uint32_t The number of indices added (used for offset to index IBO)
\brief  Generate indices for a given tile and way (i.e. road, area etc.).
***********************************************************************************************************************/
inline uint32_t generateIndices(Tile& tile, std::vector<Way>& way)
{
	uint32_t count = 0;
	for (uint32_t i = 0; i < way.size(); ++i)
	{
		for (size_t j = 0; j < way[i].nodeIds.size(); ++j)
		{
			tile.indices.push_back(tile.nodes.find(way[i].nodeIds[j])->second.index);
			count++;
		}
	}
	return count;
}

/*!*********************************************************************************************************************
\param	tile The tile to generate normals for.
\param	offset The offset into the index buffer.
\param	count The number of indices to iterate over.
\brief	Generate normals cross(b-a, c-a).
***********************************************************************************************************************/
inline void generateNormals(Tile& tile, uint32_t offset, uint32_t count)
{
	for (uint32_t i = 0; i < count; i += 3)
	{
		uint32_t a = tile.indices[offset + i];
		uint32_t b = tile.indices[offset + i + 1];
		uint32_t c = tile.indices[offset + i + 2];

		glm::vec3 N = glm::cross(tile.vertices[b].pos - tile.vertices[a].pos, tile.vertices[c].pos - tile.vertices[a].pos);

		tile.vertices[a].normal += N;
		tile.vertices[b].normal += N;
		tile.vertices[c].normal += N;
	}
}

/*!*********************************************************************************************************************
\param  tile The tile to generate indices for.
\param  way The vector of outlines to generate indices for.
\return uint32_t The number of indices added (used for offset to index IBO)
\brief  Generate indices for a given tile and outline (i.e road, area etc.).
***********************************************************************************************************************/
inline uint32_t generateIndices(Tile& tile, std::vector<uint64_t>& outlines)
{
	uint32_t count = 0;
	for (uint32_t i = 0; i < outlines.size(); ++i)
	{
		tile.indices.push_back(tile.nodes.find(outlines[i])->second.index);
		count++;
	}
	return count;
}

/*!*********************************************************************************************************************
\param  tile The tile to generate indices for.
\param  way The vector of ways to generate indices for.
\param  type The road type to generate indices for.
\return uint32_t The number of indices added (used for offset to index IBO)
\brief  Generate indices for a given tile and way - specifically for road types
(i.e motorway, primary etc.).
***********************************************************************************************************************/
inline uint32_t generateIndices(Tile& tile, std::vector<Way>& way, RoadTypes::RoadTypes type)
{
	uint32_t count = 0;
	for (uint32_t i = 0; i < way.size(); ++i)
	{
		if (way[i].roadType == type)
		{
			for (uint32_t j = 0; j < way[i].nodeIds.size(); ++j)
			{
				tile.indices.push_back(tile.nodes.find(way[i].nodeIds[j])->second.index);
				count++;
			}
		}
	}
	return count;
}

inline glm::dvec2 getMapWorldDimensions(NavDataProcess& navDataProcess, uint32_t numCols, uint32_t numRows)
{
	glm::dvec2 mapDim(navDataProcess.getTiles()[numCols - 1][numRows - 1].max - navDataProcess.getTiles()[0][0].min);
	double mapAspectRatio = mapDim.y / mapDim.x;

	double mapWorldDimX = (navDataProcess.getOsm().maxLonLat.x - navDataProcess.getOsm().minLonLat.x) * 64000; // MAGIC NUMBER: Gives you the order of magnitude of the map size.
	return glm::vec2(mapWorldDimX, mapWorldDimX * mapAspectRatio);
}

inline void remapItemCoordinates(NavDataProcess& navDataProcess, uint32_t& numCols, uint32_t& numRows, glm::dvec2& mapWorldDim)
{
	for (uint32_t col = 0; col < navDataProcess.getTiles().size(); ++col)
	{
		auto& tileCol = navDataProcess.getTiles()[col];
		for (uint32_t row = 0; row < tileCol.size(); ++row)
		{
			auto& tile = tileCol[row];

			// Set the min and max coordinates for the tile
			tile.screenMin = remap(tile.min, navDataProcess.getTiles()[0][0].min, navDataProcess.getTiles()[numCols - 1][numRows - 1].max, -mapWorldDim * .5, mapWorldDim * .5);

			tile.screenMax = remap(tile.max, navDataProcess.getTiles()[0][0].min, navDataProcess.getTiles()[numCols - 1][numRows - 1].max, -mapWorldDim * .5, mapWorldDim * .5);

			const float newMax = (float)glm::length(mapWorldDim);
			const float oldMax = (float)glm::length(navDataProcess.getTiles()[numCols - 1][numRows - 1].max);

			for (uint32_t lod = 0; lod < LOD::Count; ++lod)
			{
				// Max X extents and position
				for (uint32_t label_id = 0; label_id < tile.labels[lod].size(); ++label_id)
				{
					auto& label = tile.labels[lod][label_id];

					// Remap the position of the label.
					label.coords =
						remap(label.coords, navDataProcess.getTiles()[0][0].min, navDataProcess.getTiles()[numCols - 1][numRows - 1].max, -mapWorldDim * .5, mapWorldDim * .5);

					// Remap the previously calculated distance to the closest boundary
					// and distance to end of the road segment - for culling.
					label.distToBoundary = remap(label.distToBoundary, 0.0f, oldMax, 0.0f, newMax);
					label.distToEndOfSegment = remap(label.distToEndOfSegment, 0.0f, oldMax, 0.0f, newMax);
				}

				// Max X extents and position
				for (uint32_t amenity_label_id = 0; amenity_label_id < tile.amenityLabels[lod].size(); ++amenity_label_id)
				{
					auto& label = tile.amenityLabels[lod][amenity_label_id];

					// Remap the position of the label.
					label.coords =
						remap(label.coords, navDataProcess.getTiles()[0][0].min, navDataProcess.getTiles()[numCols - 1][numRows - 1].max, -mapWorldDim * .5, mapWorldDim * .5);

					// Remap the position of the icon.
					label.iconData.coords = remap(
						label.iconData.coords, navDataProcess.getTiles()[0][0].min, navDataProcess.getTiles()[numCols - 1][numRows - 1].max, -mapWorldDim * .5, mapWorldDim * .5);

					// Remap the previously calculated distance to the closest boundary
					// and distance to end of the road segment - for culling.
					label.distToBoundary = remap(label.distToBoundary, 0.0f, oldMax, 0.0f, newMax);
					label.distToEndOfSegment = remap(label.distToEndOfSegment, 0.0f, oldMax, 0.0f, newMax);
				}

				for (auto&& icon : tile.icons[lod])
				{
					icon.coords =
						remap(icon.coords, navDataProcess.getTiles()[0][0].min, navDataProcess.getTiles()[numCols - 1][numRows - 1].max, -mapWorldDim * .5, mapWorldDim * .5);
				}
			}
		}
	}
}

inline double calculateAngleBetweenPoints(glm::dvec2 start, glm::dvec2 end)
{
	// convert it to standard cartersian corrdinate system,
	// becasue the positive z is facing the user in opengl.
	// end.y *= -1.f;
	// start.y *= -1.f;
	double dy = end.y - start.y;
	double dx = end.x - start.x;
	double theta = std::atan2(dy, dx); // range [-PI, PI]
	theta = glm::degrees(theta); // rads to degs, range [-180, 180]
	// range [0, 360)
	if (theta < 0)
	{
		theta = 360 + theta;
	}
	return theta;
}

/*!*********************************************************************************************************************
\brief  Determine min and max coordinates of individual tiles.
***********************************************************************************************************************/
inline void NavDataProcess::initialiseTiles()
{
	// Tiles will be the same size regardless of the map
	_osm.numCols = int(glm::ceil((_osm.maxLonLat.x - _osm.minLonLat.x) / _osm.lonTileScale));
	_osm.numRows = int(glm::ceil((_osm.maxLonLat.y - _osm.minLonLat.y) / _osm.latTileScale));
	double tileScaleX = _osm.bounds.max.x / double(_osm.numCols);
	double tileScaleY = _osm.bounds.max.y / double(_osm.numRows);

	_osm.boundaryNodes.resize(_osm.numCols);

	for (uint32_t i = 0; i < _osm.numCols; ++i)
	{
		std::vector<Tile> tempCol;
		_osm.boundaryNodes[i].resize(_osm.numRows);

		for (uint32_t j = 0; j < _osm.numRows; ++j)
		{
			Tile tempTile;

			tempTile.min.x = _osm.bounds.min.x + tileScaleX * i;
			tempTile.min.y = _osm.bounds.min.y + tileScaleY * j;

			tempTile.max.x = _osm.bounds.min.x + tileScaleX * (i + 1);
			tempTile.max.y = _osm.bounds.min.y + tileScaleY * (j + 1);

			tempCol.push_back(tempTile);
		}
		_osm.tiles.push_back(tempCol);
	}
}

/*!*********************************************************************************************************************
\return	std::string	The unique ID to return.
\param	tags	A collection of tags to search.
\brief	Use the tags to find the unique ID.
***********************************************************************************************************************/
inline std::string NavDataProcess::getAttributeRef(const std::vector<Tag>& tags) const
{
	std::string value = "";
	for (auto& tag : tags)
	{
		if (tag.key.compare("ref") == 0)
		{
			value = tag.value;
			break;
		}
	}
	return value;
}

/*!*********************************************************************************************************************
\param	s	String to sanitise.
\brief	Sanitises the incoming std::string.
***********************************************************************************************************************/
inline void NavDataProcess::cleanString(std::string& s) const
{
	// Remove HTML escape character for '&'
	std::size_t pos = s.find("&amp;", 0);
	if (pos != s.npos)
	{
		std::string sub1 = s.substr(0, pos);
		sub1.append(" & ");
		std::string sub2 = s.substr(pos + 5, s.length() - 1);
		s.clear();
		s.append(sub1);
		s.append(sub2);
	}
}

/*!*********************************************************************************************************************
\return bool True if the road is a roundabout otherwise false.
\param	tags Tags for the road.
\brief	Clears data no longer needed from the osm object.
***********************************************************************************************************************/
inline bool NavDataProcess::isRoadRoundabout(const std::vector<Tag>& tags) const
{
	for (auto& tag : tags)
	{
		if (tag.key.compare("junction") == 0 && tag.value.compare("roundabout") == 0)
			return true;
	}
	return false;
}

/*!*********************************************************************************************************************
\return bool True if the road is a roundabout otherwise false.
\param	tags Tags for the road.
\brief	Clears data no longer needed from the osm object.
***********************************************************************************************************************/
inline bool NavDataProcess::isRoadOneWay(const std::vector<Tag>& tags) const
{
	for (auto& tag : tags)
	{
		if (tag.key.compare("oneway") == 0 && tag.value.compare("yes") == 0)
			return true;
	}
	return false;
}

/*!*********************************************************************************************************************
\brief	Clears data no longer needed from the osm object.
***********************************************************************************************************************/
inline void NavDataProcess::cleanData()
{
	_osm.cleanData();
}

/*!*********************************************************************************************************************
\return	Return the 2 points that intersect with the circle.
\param	centre		Centre of circle.
\param	r			Radius of circle.
\param	m			Gradient of line.
\param	constant	Constant of line.
\brief	Find the 2 intersections of a line with a circle (assumes they
intersect).
***********************************************************************************************************************/
inline std::array<glm::dvec2, 2> NavDataProcess::circleIntersects(const glm::vec2 centre, const double r, const double m, const double constant) const
{
	const double a = glm::pow(m, 2.0) + 1.0;
	const double b = 2.0 * m * (constant - centre.y) - 2.0 * centre.x;
	const double c = pow(centre.x, 2.0) + pow(constant - centre.y, 2.0) - pow(r, 2.0);

	const double x1 = (-b + sqrt(pow(b, 2.0) - 4.0 * a * c)) / (2.0 * a);
	const double x2 = (-b - sqrt(pow(b, 2.0) - 4.0 * a * c)) / (2.0 * a);

	return std::array<glm::dvec2, 2>{ glm::dvec2(x1, m * x1 + constant), glm::dvec2(x2, m * x2 + constant) };
}

/*!*********************************************************************************************************************
\return	pvr::PolygonWindingOrder	FrontFaceCW (clockwise) or
FrontFaceCCW (anti-clockwise).
\param	points	The series of points to check.
\brief	Checks the winding order of a a series of points.
***********************************************************************************************************************/
inline pvr::PolygonWindingOrder NavDataProcess::checkWinding(const std::vector<glm::dvec2>& points) const
{
	const double area = calculateTriangleArea(points);

	if (area <= 0.0)
		return pvr::PolygonWindingOrder::FrontFaceCCW;
	else
		return pvr::PolygonWindingOrder::FrontFaceCW;
}

/*!*********************************************************************************************************************
\return	Find intersection point of 2 lines.
\param	p1	Point on line 1.
\param	d1	Direction vector of line 1.
\param	p2	Point on line 2.
\param	d2	Direction vector of line 2.
\brief	Find intersection point of 2 lines (assumes they intersect).
***********************************************************************************************************************/
inline glm::dvec2 NavDataProcess::lineIntersect(const glm::dvec2 p1, const glm::dvec2 d1, const glm::dvec2 p2, const glm::dvec2 d2) const
{
	if (compareReal(p1.x, p2.x) && compareReal(p1.y, p2.y))
		return p1;

	double num = glm::length(glm::cross(glm::dvec3(p2 - p1, 0), glm::dvec3(d2, 0)));
	double denom = glm::length(glm::cross(glm::dvec3(d1, 0), glm::dvec3(d2, 0)));

	if (denom == 0.0)
		return p1;

	double a = num / denom;
	return p1 + a * d1;
}

inline void NavDataProcess::processLabelBoundary(LabelData& label, glm::uvec2& tileCoords)
{
	glm::dvec2 min = _osm.tiles[tileCoords.x][tileCoords.y].min;
	glm::dvec2 max = _osm.tiles[tileCoords.x][tileCoords.y].max;

	glm::dvec2 leftBoundary = glm::dvec2(findIntersect(min, max, label.coords, label.coords - glm::dvec2(max.x * 2.0, 0)));
	glm::dvec2 rightBoundary = glm::dvec2(findIntersect(min, max, label.coords, label.coords + glm::dvec2(max.x * 2.0, 0)));
	glm::dvec2 topBoundary = glm::dvec2(findIntersect(min, max, label.coords, label.coords + glm::dvec2(0, max.y * 2.0)));
	glm::dvec2 bottomBoundary = glm::dvec2(findIntersect(min, max, label.coords, label.coords - glm::dvec2(0, max.y * 2.0)));
	double d1 = glm::distance2(leftBoundary, label.coords);
	double d2 = glm::distance2(rightBoundary, label.coords);
	double d3 = glm::distance2(topBoundary, label.coords);
	double d4 = glm::distance2(bottomBoundary, label.coords);

	label.distToBoundary = static_cast<float>(glm::sqrt(glm::min(glm::min(glm::min(d1, d2), d3), d4)));
}

/*!*********************************************************************************************************************
\return Return true if the point is out of bounds.
\brief  Find if the point is out of the map bounds.
***********************************************************************************************************************/
inline bool NavDataProcess::isOutOfBounds(const glm::dvec2& point) const
{
	return ((point.x < _osm.bounds.min.x) || (point.y < _osm.bounds.min.y) || (point.x > _osm.bounds.max.x) || (point.y > _osm.bounds.max.y));
}

inline float calulateTimeInMillisec(float distance, float speed)
{
	return distance / speed * 1000.f;
}

// Calculate the key frametime between one point to another.
inline float calculateRouteKeyFrameTime(const glm::dvec2& start, const glm::dvec2& end, float totalDistance, float speed)
{
	double dist = glm::distance(start, end);
	return static_cast<float>(calulateTimeInMillisec(totalDistance, speed) * dist / totalDistance);
}
