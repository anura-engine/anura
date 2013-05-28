# merge.py: Merge two or more Anura levels. Script by DDR0.
# Input: FML-formatted levels (plain text). 
# Output: One FSON-formatted level, in plain text. The zorders you request will be set beside each other on the level, with some space left for padding. Relative layer positioning is preserved.
# Flags: --zorder int[ int[ …]]: Specifies the zorders of the tiles you want to copy. Defaults to "all".
#        --input [file[ file[ …]]]: Read the levels from this file. Defaults to reading input piped to it.
#        --output file: Writes the output to the file. Defaults to simply piping output onward.
#        --help: Print a help message and exit.

import sys, os, datetime

### DEFINE FUNCTIONS ###

def printHelp(args):
	help="""merge.py is a python 3.x script that takes old-style levels and copies their
  tiles in to a new-style level.
Input: FML-formatted levels (plain text). 
Output: One FSON-formatted level, in plain text. The zorders you request will
  be set beside each other on the level, with some space left for padding.
  Relative layer positioning is preserved.
Flags: --zorder int[ int[ …]]: Specifies the zorders of the tiles you want to
                               copy. Defaults to "all".
       --input [file[ file[ …]]]: Read the levels from this file. Defaults to
                                  reading input piped to it.
       --output file: Writes the output to the file. Defaults to simply piping
                      output onward.
       --help: Print this help message and exit.
For example, to merge tiles with zorders 4 and 5 from 71_bable.cfg and
  56_DDR06_Mt_Upriayr_Road.cfg and write the output to out.cfg, you'd run:
python old_tile_map_merge.py --zorder 5 4 --input levels/71_bable.cfg levels/56_DDR06_Mt_Upriayr_Road.cfg --output out.cfg > out.cfg"""
	sys.exit(help)

def parseArgs(args):
	global output
	
	def eatZorder(args):
		global zorders
		zorders += [int(args[0])]
		if(args[1:] and args[1][:2] != '--'):
			eatZorder(args[1:])
		else:
			parseArgs(args[1:])
			
	def eatInput(args):
		global levels
		if(not os.path.exists(args[0])):
			raise Exception("Could not find file " + args[0])
		levels += [args[0]]
		if(args[1:] and args[1][:2] != '--'):
			eatInput(args[1:])
		else:
			parseArgs(args[1:])
	
	if(len(args)):
		if(args[0] == '--zorder'):
			eatZorder(args[1:])
		elif(args[0] == '--input'):
			eatInput(args[1:])
		elif(args[0] == '--output'):
			output = args[1]
			parseArgs(args[2:])
		elif(args[0] == '--help' or args[0] == '--h'):
			printHelp()
		else:
			sys.exit('Error: Unrecognised arg '+args[0]+'. Args are --zorder; --input; --output and --help, which you probably need.')

def extractFromTo(startTag, endTag, data):
	found = []
	openAt = data.find(startTag)
	while openAt != -1:
		closeAt = data.find(endTag, openAt+len(startTag))
		found += [data[openAt+len(startTag):closeAt]]
		openAt = data.find(startTag, closeAt+len(endTag))
	return found

def extractTags(tagName, data): return extractFromTo('['+tagName+']', '[/'+tagName+']', data)
def extractChar(character, data): return extractFromTo('character', 'character', data)

def parseTagContents(data):
	dictionary = {}
	position = 0
	while data[position:] and not data[position:].isspace(): #Check we've still got something to process.
		equalsPos = data.find('=', position)
		key = data[position:equalsPos].strip()
		firstQuotePos = data.find('"', equalsPos+1)
		secondQuotePos = data.find('"', firstQuotePos+1)
		value = data[firstQuotePos+1:secondQuotePos].strip()
		dictionary[key]=value
		position = secondQuotePos+1
	return dictionary
	
def parseTileMapsInLevels(levels):
	levels = list(map(lambda x: list(x), levels)) #We have to explicitly unpack the levels here, as we can only iterate over them once.
	levelsToReturn = []
	for level in levels:
		for tilemap in level:
			tilemap['tiles'] = map(lambda row: row.split(","), tilemap['tiles'].split("\n"))
		levelsToReturn += [list(level)]
	return levels
	
def calculateDimensions(data):
	minX = maxX = minY = maxY = 0
	for tilemap in data:
		tilemap['tiles'] = list(map(lambda x: list(x), tilemap['tiles']))
		width = 0
		for line in tilemap['tiles']:
			width = max(width, len(line))
		height = len(tilemap['tiles'])
		minX = min(int(int(tilemap['x'])/16), minX) #16 is the width/height of an Anura tile. Data is given in pixels, so we need to divide.
		minY = min(int(int(tilemap['y'])/16), minY)
		maxX = max(int(int(tilemap['x'])/16) + width, maxX)
		maxY = max(int(int(tilemap['y'])/16) + height, maxY)
		#print('height:', height, 'minx', minX, 'miny', minY, 'maxx', maxX, 'maxy', maxY)
	return {'minX':minX, 'maxX':maxX, 'minY':minY, 'maxY':maxY, 'maps':data}
	
def combineTileMaps(data):
	def squareArrayTo(array, width, height):
		array += [[] for x in range(height-len(array))]
		for line in array:
			line += ['' for y in range(width-len(line))]
	
	zs = []
	data = list(data)
	for files in data:
		for tilemap in files['maps']:
			tilemapz = int(tilemap.get('zorder', 0))
			if(not tilemapz in zs):
				zs += [tilemapz]
	zs.sort() #The zindexs will be our index from 0 of the output list.
	
	compiledMaps = [{'w':0, 'h':0, 'z':0, 'data':[]} for x in range(len(zs))]
	for files in data:
		f_offsetX = files['minX']
		f_offsetY = files['minY']
		f_width = files['maxX'] - files['minX']
		f_height = files['maxY'] - files['minY']
		for tilemap in files['maps']:
			mapIndex = zs.index(int(tilemap['zorder']))
			mapToAddTo = compiledMaps[mapIndex]
			mapToAddTo['z'] = int(tilemap['zorder'])
			mapToAddTo['h'] = max(mapToAddTo['h'], f_height)
			squareArrayTo(mapToAddTo['data'], mapToAddTo['w'], mapToAddTo['h'])
			for lineNo in range(len(tilemap['tiles'])):
				mapToAddTo['data'][lineNo] += tilemap['tiles'][lineNo]
			mapToAddTo['w'] += f_width + 5 #Make a five-tile margin so that boundaries are easy to select.
	return compiledMaps
	
def tilemapTostring(tilemap):
	returnString = '\t"x_speed": 100,\n\t"y_speed": 100,\n\t"x": 0,\n\t"y": 0,\n\t"zorder": ' + str(tilemap['z']) + ',\n\t"tiles": "'
	uniqueTiles = set([])
	for line in tilemap['data']:
		for tile in line:
			uniqueTiles.add(tile)
		returnString += ','.join(line)+'\n'
	returnString += '",\t\n"unique_tiles": "'+','.join(uniqueTiles)+'",'
	return returnString

### CALCULATE STUFF ###

zorders = []
levels = []
output = ""		

parseArgs(sys.argv[1:])

rawLevelData = [] #Contains each level we selected for parsing as one long string.
if(levels):
	rawLevelData = map(lambda lName: open(lName).read(), levels)
else:
	rawLevelData = [sys.stdin.read()]

levelTileMapTagsRawContents = map(lambda levelString: extractTags('tile_map', levelString), rawLevelData) #levelTileMapTagsRawContents is a list of lists of each rawLevelData's tilemap tag contents.

levelTileMapTagsContents = map(lambda levelMapTags: map(parseTagContents, levelMapTags), levelTileMapTagsRawContents) #Yeilds a list of maps containing key/value pairs.

filteredTileMaps = [] #We now know enough to filter out the tile maps that we don't care about. If we have any such maps.
if(zorders):
	filteredTileMaps = filter(lambda I: I, map(lambda levelMaps: filter(lambda tmap: int(tmap.get('zorder', 0)) in zorders, levelMaps), levelTileMapTagsContents))
else:
	filteredTileMaps = levelTileMapTagsContents

parsedFilteredTileMaps = parseTileMapsInLevels(filteredTileMaps) #Turns the tiles maps we have from strings into a list of lists of tiles.
	
boundedTileMaps = map(calculateDimensions, parsedFilteredTileMaps)

reducedTileMap = combineTileMaps(boundedTileMaps)

### PRINT RESULTS ###

lWidth = 0
for tmp in reducedTileMap:
	lWidth = max(lWidth, tmp['w'])
lWidth *= 16
lHeight = 0
for tmp in reducedTileMap:
	lHeight = max(lHeight, tmp['h'])
lHeight *= 16

tilemapStrings = list(map(tilemapTostring, reducedTileMap))

print(
'//Generated by merge.py on {now.year}/{now.month}/{now.day} at {now.hour}:{now.minute}. Session args were: '.format(now=datetime.datetime.now())+str(sys.argv[1:])+'.'
+"""
{
air_resistance: 20,
auto_move_camera: [0,0],
dimensions: [0,0,"""+str(lWidth*2)+','+str(lHeight*2)+"""],
id: \""""+output+"""\",
music: "",
preloads: "",
segment_height: 0,
segment_width: 0,
gui: "null",
tile_map: [{
"""+"""
},{
""".join(tilemapStrings)+"""
}],
title: "",
version: 1.2,
water_resistance: 100,
xscale: 100,
yscale: 100,
serialized_objects: {
},
}""")
