#!/usr/bin/env python
# Portions Copyright (C) 2001 Artifex Software Inc.
#   
#   This software is distributed under license and may not be copied, modified
#   or distributed except as expressly authorized under the terms of that
#   license.  Refer to licensing information at http://www.artifex.com/ or
#   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
#   San Rafael, CA  94903, (415)492-9861, for further information. 
#
# TODO
# array data should be wrapped.
#
# text data should be printed as a string not an array of ascii values.
#
# enumerations should printed we now print the ordinal value of the enumeration.
#
# make self.unpack endian like with binding

# DIFFS between HP
# Artifex reports the file offset of each operator HP does not.

import string 

# for packing and unpacking binary data
from struct import *

# tags
pxl_tags_dict = {
    'ArcPath' :                 0x91,
    'BeginChar' :               0x52,
    'BeginFontHeader' :         0x4f,
    'BeginImage' :              0xb0,
    'BeginPage' :               0x43,
    'BeginRastPattern' :        0xb3,
    'BeginScan' :               0xb6,
    'BeginSession' :            0x41,
    'BeginStream' :             0x5b,
    'BezierPath' :              0x93,
    'BezierRelPath' :           0x95,
    'Chord' :                   0x96,
    'ChordPath' :               0x97,
    'CloseDataSource' :         0x49,
    'CloseSubPath' :            0x84,
    'Comment' :                 0x47,
    'Ellipse' :                 0x98,
    'EllipsePath' :             0x99,
    'EndChar' :                 0x54,
    'EndFontHeader' :           0x51,
    'EndImage' :                0xb2,
    'EndPage' :                 0x44,
    'EndRastPattern' :          0xb5,
    'EndScan' :                 0xb8,
    'EndSession' :              0x42,
    'EndStream' :               0x5d,
    'ExecStream' :              0x5e,
    'LinePath' :                0x9b,
    'LineRelPath' :             0x9d,
    'NewPath' :                 0x85,
    'OpenDataSource' :          0x48,
    'PaintPath' :               0x86,
    'Pie' :                     0x9e,
    'PiePath' :                 0x9f,
    'PopGS' :                   0x60,
    'PushGS' :                  0x61,
    'ReadChar' :                0x53,
    'ReadFontHeader' :          0x50,
    'ReadImage' :               0xb1,
    'ReadRastPattern' :         0xb4,
    'ReadStream' :              0x5c,
    'Rectangle' :               0xa0,
    'RectanglePath' :           0xa1,
    'RemoveFont' :              0x55,
    'SetCharAttributes' :       0x56,
    'RemoveStream' :            0x5f,
    'RoundRectangle' :          0xa2,
    'RoundRectanglePath' :      0xa3,
    'ScanLineRel' :             0xb9,
    'SetClipReplace' :          0x62,
    'SetBrushSource' :          0x63,
    'SetCharAngle' :            0x64,
    'SetCharBoldValue' :        0x7d,
    'SetCharScale' :            0x65,
    'SetCharShear' :            0x66,
    'SetCharSubMode' :          0x81,
    'SetClipIntersect' :        0x67,
    'SetClipMode' :             0x7f,
    'SetClipRectangle' :        0x68,
    'SetClipToPage' :           0x69,
    'SetColorSpace' :           0x6a,
    'SetCursor' :               0x6b,
    'SetCursorRel' :            0x6c,
    'SetHalftoneMethod' :       0x6d,
    'SetFillMode' :             0x6e,
    'SetFont' :                 0x6f,
    'SetLineCap' :              0x71,
    'SetLineDash' :             0x70,
    'SetLineJoin' :             0x72,
    'SetMiterLimit' :           0x73,
    'SetPageDefaultCTM' :       0x74,
    'SetPageOrigin' :           0x75,
    'SetPageRotation' :         0x76,
    'SetPageScale' :            0x77,
    'SetPathToClip' :           0x80,
    'SetPatternTxMode' :        0x78,
    'SetPenSource' :            0x79,
    'SetPenWidth' :             0x7a,
    'SetROP' :                  0x7b,
    'SetSourceTxMode' :         0x7c,
    'Text' :                    0xa8,
    'TextPath' :                0xa9,
    'attr_ubyte' :              0xf8,
    'attr_uint16' :             0xf9,
    'embedded_data' :           0xfa,
    'embedded_data_byte' :      0xfb,
    'real32' :                  0xc5,
    'real32_array' :            0xcd,
    'real32_box' :              0xe5,
    'real32_xy' :               0xd5,
    'sint16' :                  0xc3,
    'sint16_array' :            0xcb,
    'sint16_box' :              0xe3,
    'sint16_xy' :               0xd3,
    'sint32' :                  0xc4,
    'sint32_array' :            0xcc,
    'sint32_box' :              0xe4,
    'sint32_xy' :               0xd4,
    'ubyte' :                   0xc0,
    'ubyte_array' :             0xc8,
    'ubyte_box' :               0xe0,
    'ubyte_xy' :                0xd0,
    'uint16' :                  0xc1,
    'uint16_array' :            0xc9,
    'uint16_box' :              0xe1,
    'uint16_xy' :               0xd1,
    'uint32' :                  0xc2,
    'uint32_array' :            0xca,
    'uint32_box' :              0xe2,
    'uint32_xy' :               0xd2
}

# nb enumerations are not supported we print the ordinal value.
pxl_enumerations_dict = {
    'DataOrg' : [ 'eBinaryHighByteFirst', 'eBinaryLowByteFirst' ],
    'DataSource' : [ 'eDefault' ],
    'DataType' : [ 'eUByte', 'eSByte', 'eUint16', 'eSint16' ],
    'ArcDirection' : [ 'eClockWise',  'eCounterClockWise' ],
    'DitherMatrix' : [ 'eDeviceBest' ],
    'CharSubModeArray' : [ 'eNoSubstitution', 'eVerticalSubstitution' ],
    'ClipMode' : ['eNonZeroWinding', 'eEvenOdd' ],
    'DuplexPageSide' : [ 'eFrontMediaSide', 'eBackMediaSide' ],
    'ClipRegion' : ['eInterior', 'eExterior'],
    'ErrorReport' : ['eNoReporting', 'eBackChannel'],
    'ColorDepth' : [ 'e1Bit', 'e4Bit', 'e8Bit' ],
    'ColorMapping' : [ 'eDirectPixel', 'eIndexedPixel' ],
    'ColorSpace' : [ 'eGray', 'eRGB', 'eSRGB' ],
    'CompressMode' : [ 'eNoCompression', 'eRLECompression', 'eJPEGCompression' ],
    'LineJoineMiterJoin' : [ 'eRoundJoin', 'eBevelJoin', 'eNoJoin' ],
    'MeasureeInch' : [ 'eMillimeter', 'eTenthsOfAMillimeter' ],
    'MediaSize' : [ 'eLetterPaper', 'eLegalPaper', 'eA4Paper',
		    'eExecPaper', 'eLedgerPaper', 'eA3Paper', 
		    'eCOM10Envelope', 'eMonarchEnvelope', 'eC5Envelope', 
		    'eDLEnvelope', 'eJB4Paper', 'eJB5Paper', 'eB5Envelope',
		    'eJPostcard', 'eJDoublePostcard', 'eA5Paper', 'eA6Paper',
		    'eJBPaper' ],
    'MediaSource' : [ 'eDefaultSource', 'eAutoSelect', 'eManualFeed',
		      'eMultiPurposeTray', 'eUpperCassette', 'eLowerCassette', 
		      'eEnvelopeTray', 'eThirdCassette' ],
    'MediaDestination' :  [ 'eDefaultDestination', 'eFaceDownBin', 'eFaceUpBin',
			     'eJobOffsetBin' ],
    'DuplexPageMode' : [ 'eDuplexHorizontalBinding', 'eDuplexVerticalBinding' ],
    'DuplexPageSide' : [ 'eFrontMediaSide', 'eBackMediaSide' ],
    'ErrorReport' :  [ 'eNoReporting', 'eBackChannel' ],
    'BackCh' : ['eErrorPage'],
    'ErrPage' : ['eBackChAndErrPage', 'eNWBackChannel', 'eNWErrorPage',
		 'eNWBackChAndErrPage'],
    'FillMode' : ['eNonZeroWinding', 'eEvenOdd' ],
    'LineCapeButtCap' : [ 'eRoundCap', 'eSquareCap', 'eTriangleCap' ],
    'LineJoineMiterJoin' : [ 'ePortraitOrientation', 'eLandscapeOrientation',
			     'eReversePortrait', 'eReverseLandscape' ],
    'PatternPersistence' : [ 'eTempPattern', 'ePagePattern', 'eSessionPattern'],
    'SimplexPageMode' : ['eSimplexFrontSide'],
    'TxMode' : [ 'eOpaque', 'eTransparent' ],
    'WritingMode' : [ 'eHorizontal', 'eVertical' ]
}

# see appendix F
pxl_attribute_name_to_attribute_number_dict = { 
    'ArcDirection' : 65,
    'BlockByteLength' : 111,
    'BlockHeight' : 99,
    'BoldValue' : 177,
    'BoundingBox' : 66,
    'ColorimetricColorSpace': 17,
    'CharAngle' : 161,
    'CharCode' : 162,
    'CharDataSize' : 163,
    'CharScale' : 164,
    'CharShear' : 165,
    'CharSize' : 166,
    'CharSubModeArray' : 172,
    'ClipMode' : 84,
    'ClipRegion' : 83,
    'ColorDepth' : 98,
    'ColorMapping' : 100,
    'ColorSpace' : 3,
    'CommentData' : 129,
    'CompressMode' : 101,
    'ControlPoint1' : 81,
    'ControlPoint2' : 82,
    'CRGBMinMax' : 20,
    'CustomMediaSize' : 47,
    'CustomMediaSizeUnits' : 48,
    'DashOffset' : 67,
    'DataOrg' : 130,
    'DestinationBox' : 102,
    'DestinationSize' : 103,
    'DeviceMatrix' : 33,
    'DitherMatrixDataType' : 34,
    'DitherMatrixDepth' : 51,
    'DitherMatrixSize' : 50,
    'DitherOrigin' : 35,
    'DuplexPageMode' : 53,
    'DuplexPageSide' : 54,
    'EllipseDimension' : 68,
    'EndPoint' : 69,
    'ErrorReport' : 143,
    'FillMode' : 70,
    'FontFormat' : 169,
    'FontHeaderLength' : 167,
    'FontName' : 168,
    'GammaGain' : 21,
    'GrayLevel' : 9,
    'LineCapStyle' : 71,
    'LineDashStyle' : 74,
    'LineJoinStyle' : 72,
    'Measure' : 134,
    'MediaSize' : 37,
    'MediaSource' : 38,
    'MediaType' : 39,
    'MiterLength' : 73,
    'NewDestinationSize' : 13,
    'NullBrush' : 4,
    'NullPen' : 5,
    'NumberOfPoints' : 77,
    'NumberOfScanLines' : 115,
    'Orientation' : 40,
    'PadBytesMultiple' : 110,
    'PageAngle' : 41,
    'PageCopies' : 49,
    'PageOrigin' : 42,
    'PageScale' : 43,
    'PaletteData' : 6,
    'PaletteDepth' : 2,
    'PatternDefineID' : 105,
    'PatternOrigin' : 12,
    'PatternPersistence' : 104,
    'PatternSelectID' : 8,
    'PenWidth' : 75,
    'Point' : 76,
    'PointType' : 80,
    'PrimaryArray' : 14,
    'PrimaryDepth' : 15,
    'RGBColor' : 11,
    'ROP3' : 44,
    'SimplexPageMode' : 52,
    'SolidLine' : 78,
    'SourceHeight' : 107,
    'SourceType' : 136,
    'SourceWidth' : 108,
    'StartLine' : 109,
    'StartPoint' : 79,
    'StreamDataLength' : 140,
    'StreamName' : 139,
    'SymbolSet' : 170,
    'TextData' : 171,
    'TxMode' : 45,
    'UnitsPerMeasure' : 137,
    'WhiteReferencePoint' : 19,
    'WritingMode' : 173,
    'XSpacingData' : 175,
    'XYChromaticities' : 18,
    'YSpacingData' : 176,
}

class pxl_dis:

    def __init__(self, data):
        # the class does not handle pjl, work around that here
        index = 0
        while( data[index] != ')' and  data[index] != '(' ):
            index = index + 1
        
        # copy of the data without the PJL
        data = data[index:]
        self.data = data
        
        # parse out data order and protocol
        self.binding = data[0]
        self.protocol = data[12]
        self.revision = data[14]

        # check binding NB - should check other stuff too:
        # example: )<SP>HP-PCL XL;2;0<CR><LF>
        if self.binding not in ['(', ')', '`']:
            raise(SyntaxError)

        # pointer to data
        self.index = 0

        # skip over protocol and revision
        while( data[self.index] != '\n' ):
            self.index = self.index + 1
        self.index = self.index + 1

        # print out ascii protocol and revision.  NB should check
        # revisions are the same.
        print "` HP-PCL XL;2;0"
        # saved size of last array parsed
        self.size_of_element = -1;
        self.size_of_array = -1;
        self.unpack_string = ""

        # dictionary of streams keyed by stream name
	self.user_defined_streams = {}

        # the n'th operator in the stream
        self.operator_position = 0
        
    def big_endian_stream(self):
        return (self.binding == ')')

    def little_endian_stream(self):
        return (self.binding == '(')

    def ascii_stream(self):
        return (self.binding == '`')

    def nullAttributeList(self):
        return 0
    
    # redefine unpack to handle endiannes
    def unpack(self, format, data):

        # prepend endian specifiers to stream if necessary.  NB we
        # don't handle ascii streams and the endian formatting does
        # not work properly right now:

        # if ( self.big_endian_stream() ):
        #
        # format = '>' + format
        # else:
        # format = '<' + format

        num = unpack(format, data)
        return num
    
    # implicitly read when parsing the tag
    def attributeIDValue(self):
        return 1

    # get the next operator
    def operatorTag(self):
        self.operator_position = self.operator_position + 1
        tag = unpack('B', self.data[self.index] )[0]
        for k in pxl_tags_dict.keys():
            if ( pxl_tags_dict[k] == tag ):
                print "// Operator Position: %d File Offset of operator" % \
                      self.operator_position, self.begin_attribute_pos
                print k
                self.index = self.index + 1
                # handle special cases
                if ( self.is_Embedded(k) ):
                    self.process_EmbeddedInfo(k)
                return 1
        return 0

    def Tag_ubyte(self):
        new_tag = unpack('B', self.data[self.index])[0]
        if ( new_tag == pxl_tags_dict['ubyte'] ):
            self.index = self.index + 1
            print "ubyte",
            self.unpack_string = 'B'
            self.size_of_element = 1
            return 1
        return 0

    def Tag_sint16(self):
        new_tag = unpack('B', self.data[self.index])[0]
        if ( new_tag == pxl_tags_dict['sint16'] ):
            self.index = self.index + 1
            print "sint16", 
            self.unpack_string = 'h'
            self.size_of_element = 2
            return 1
        return 0

    def Tag_uint16(self):
        new_tag = unpack('B', self.data[self.index])[0]
        if ( new_tag == pxl_tags_dict['uint16'] ):
            self.index = self.index + 1
            print "uint16",
            self.unpack_string = 'H'
            self.size_of_element = 2
            return 1
        return 0

    def Tag_sint32(self):
        new_tag = unpack('B', self.data[self.index])[0]

        if ( new_tag == pxl_tags_dict['sint32'] ):
            self.index = self.index + 1
            print "sint32",
            self.unpack_string = 'l'
            self.size_of_element = 4
            return 1
        return 0

    def Tag_uint32(self):
        new_tag = unpack('B', self.data[self.index])[0]

        if ( new_tag == pxl_tags_dict['uint32'] ):
            self.index = self.index + 1

            print "uint32",
            self.unpack_string = 'L'
            self.size_of_element = 4
            return 1
        return 0

    def Tag_real32(self):
        new_tag = unpack('B', self.data[self.index])[0]

        if ( new_tag == pxl_tags_dict['real32'] ):
            self.index = self.index + 1
            print "real32",
            self.unpack_string = 'f'
            self.size_of_element = 4
            return 1
        return 0

    def Tag_ubyte_array(self):
        new_tag = unpack('B', self.data[self.index])[0]

        if ( new_tag == pxl_tags_dict['ubyte_array'] ):
            self.index = self.index + 1
            self.unpack_string = 'B'
            self.size_of_element = 1
            print "ubyte_array [",
            return 1
        return 0

    def Tag_uint16_array(self):
        new_tag = unpack('B', self.data[self.index])[0]

        if ( new_tag == pxl_tags_dict['uint16_array'] ):
            self.index = self.index + 1
            self.unpack_string = 'H'
            self.size_of_element = 2
            print "uint16_array [",
            return 1
        return 0

    def Tag_sint16_array(self):
        new_tag = unpack('B', self.data[self.index])[0]

        if ( new_tag == pxl_tags_dict['sint16_array'] ):
            self.index = self.index + 1
            self.unpack_string = 'h'
            self.size_of_element = 2
            print "sint16_array [",
            return 1
        return 0

    def Tag_uint32_array(self):
        new_tag = unpack('B', self.data[self.index])[0]

        if ( new_tag == pxl_tags_dict['uint32_array'] ):
            self.index = self.index + 1
            self.unpack_string = 'L'
            self.size_of_element = 4
            print "uint32_array [",
            return 1
        return 0

    def Tag_sint32_array(self):
        new_tag = unpack('B', self.data[self.index])[0]

        if ( new_tag == pxl_tags_dict['sint32_array'] ):
            self.index = self.index + 1
            self.unpack_string = 'l'
            self.size_of_element = 4
            print "sint32_array [",
            return 1
        return 0

    def Tag_real32_array(self):
        new_tag = unpack('B', self.data[self.index])[0]

        if ( new_tag == pxl_tags_dict['real32_array'] ):
            self.index = self.index + 1
            self.unpack_string = 'f'
            self.size_of_element = 4
            print "real32_array [",
            return 1
        return 0
    
    def Tag_ubyte_xy(self):
        new_tag = unpack('B', self.data[self.index])[0]

        if ( new_tag == pxl_tags_dict['ubyte_xy'] ):
            self.index = self.index + 1

            print "ubyte_xy %d %d" % \
                  self.unpack('BB', self.data[self.index:self.index+2]),
            self.index = self.index + 2
            return 1
        return 0

    def Tag_uint16_xy(self):
        new_tag = unpack('B', self.data[self.index])[0]

        if ( new_tag == pxl_tags_dict['uint16_xy'] ):
            self.index = self.index + 1

            print "uint16_xy %d %d" % \
                  self.unpack('HH', self.data[self.index:self.index+4]),
            self.index = self.index + 4
            return 1
        return 0

    def Tag_sint16_xy(self):
        new_tag = unpack('B', self.data[self.index])[0]

        if ( new_tag == pxl_tags_dict['sint16_xy'] ):
            self.index = self.index + 1

            print "sint16_xy %d %d" % \
                  self.unpack('hh', self.data[self.index:self.index+4]),
            self.index = self.index + 4
            return 1
        return 0

    def Tag_uint32_xy(self):
        new_tag = unpack('B', self.data[self.index])[0]

        if ( new_tag == pxl_tags_dict['uint32_xy'] ):
            self.index = self.index + 1

            print "uint32_xy" % \
                  self.unpack('LL', self.data[self.index:self.index+8]),
            self.index = self.index + 8
            return 1
        return 0

    def Tag_sint32_xy(self):
        new_tag = unpack('B', self.data[self.index])[0]

        if ( new_tag == pxl_tags_dict['sint32_xy'] ):
            self.index = self.index + 1

            print "sint32_xy %d %d" % \
                  self.unpack('ll', self.data[self.index:self.index+8]),
            self.index = self.index + 8
            return 1
        return 0

    def Tag_real32_xy(self):
        new_tag = unpack('B', self.data[self.index])[0]

        if ( new_tag == pxl_tags_dict['real32_xy'] ):
            self.index = self.index + 1

            print "real32_xy %f %f" % \
                  self.unpack('ff', self.data[self.index:self.index+8]),
            self.index = self.index + 8
            return 1
        return 0
    
    def Tag_ubyte_box(self):
        new_tag = unpack('B', self.data[self.index])[0]

        if ( new_tag == pxl_tags_dict['ubyte_box'] ):
            self.index = self.index + 1

            print "ubyte_box %d %d %d %d" % \
                  self.unpack('BBBB', self.data[self.index:self.index+4]),
            self.index = self.index + 4
            return 1
        return 0

    def Tag_uint16_box(self):
        new_tag = unpack('B', self.data[self.index])[0]

        if ( new_tag == pxl_tags_dict['uint16_box'] ):
            self.index = self.index + 1
            print "uint16_box %d %d %d %d" % \
                  self.unpack('hhhh', self.data[self.index:self.index+8]),
            self.index = self.index + 8
            return 1
        return 0

    def Tag_sint16_box(self):
        new_tag = unpack('B', self.data[self.index])[0]
        if ( new_tag == pxl_tags_dict['sint16_box'] ):
            self.index = self.index + 1
            print "sint16_box %d %d %d %d" % \
                  self.unpack('hhhh', self.data[self.index:self.index+8])
            self.index = self.index + 8
            return 1
        return 0

    def Tag_uint32_box(self):
        new_tag = unpack('B', self.data[self.index])[0]
        if ( new_tag == pxl_tags_dict['uint32_box'] ):
            self.index = self.index + 1
            print "uint32_box %d %d %d %d" % \
                  self.unpack('LLLL', self.data[self.index:self.index+16])
            self.index = self.index + 32
            return 1
        return 0

    def Tag_sint32_box(self):
        new_tag = unpack('B', self.data[self.index])[0]

        if ( new_tag == pxl_tags_dict['sint32_box'] ):
            self.index = self.index + 1

            print "sint32_box %d %d %d %d" % \
                  self.unpack('llll', self.data[self.index:self.index+16])
            self.index = self.index + 16
            return 1
        return 0

    def Tag_real32_box(self):
        new_tag = unpack('B', self.data[self.index])[0]

        if ( new_tag == pxl_tags_dict['real32_box'] ):
            self.index = self.index + 1

            print "real32_box %f %f %f %f" % \
                  self.unpack('ffff', self.data[self.index:self.index+16])
            self.index = self.index + 16
            return 1
        return 0
    
    # check for embedded tags.
    def is_Embedded(self, name):
        return ( name == 'embedded_data' or name == 'embedded_data_byte' )

    def process_EmbeddedInfo(self, name):
        if ( name == 'embedded_data' ):
            # can't index into the data with a long - have to make it
            # an int.
            length = int(self.unpack('L', self.data[self.index:self.index+4])[0])
            self.index = self.index + 4
        if ( name == 'embedded_data_byte' ):
            length = int(self.unpack('B', self.data[self.index:self.index+1])[0])
            self.index = self.index + 1

        # NB needs wrapping
        print "[ ",
        for byte in self.data[self.index:self.index+length]:
            print ord(byte),
        print " ]"
        # for some screwed up reason trailing nulls can comme after
        # embedded data
        self.index = self.index + length
        while ( 1 ):
            if ( self.data[self.index] == "\0" ):
                self.index = self.index + 1
            else:
                break
            
                 
    def Tag_attr_ubyte(self):
        new_tag = unpack('B', self.data[self.index])[0]

        if ( new_tag == pxl_tags_dict['attr_ubyte'] ):
            self.index = self.index + 1

            tag = unpack('B', self.data[self.index] )[0]
            for k in pxl_attribute_name_to_attribute_number_dict.keys():
                if ( pxl_attribute_name_to_attribute_number_dict[k] == tag ):
                    print k
                    self.index = self.index + 1
		    # handle special cases
		    if ( self.is_Embedded(k) ):
			self.process_EmbeddedInfo(k)
                    return 1
        return 0

    def Tag_attr_uint16(self):
        new_tag = unpack('B', self.data[self.index])[0]
        if ( new_tag == pxl_tags_dict['attr_uint16'] ):
            self.index = self.index + 1
            print "Attribute tag uint16 # NOT IMPLEMENTED #", self.unpack('HH', self.data[self.index] )
            self.index = self.index + 2
            return 1
        return 0

    def attributeID(self):
        return (self.Tag_attr_ubyte() or self.Tag_attr_uint16()) and self.attributeIDValue()

    def singleValueType(self):
        if ( self.Tag_ubyte() or self.Tag_uint16() or self.Tag_uint32() or \
             self.Tag_sint16() or self.Tag_sint32() or self.Tag_real32() ):
            size = self.size_of_element
            if ( self.unpack_string == 'f' ):
                print "%f" % self.unpack(self.unpack_string, self.data[self.index:self.index+size]),
            else:
                print "%d" % self.unpack(self.unpack_string, self.data[self.index:self.index+size]),
            self.index = self.index + self.size_of_element
            return 1
        return 0

    def xyValueType(self):
        return self.Tag_ubyte_xy() or self.Tag_uint16_xy() or self.Tag_uint32_xy() or \
               self.Tag_sint16_xy() or self.Tag_sint32_xy() or self.Tag_real32_xy()
        
    def boxValueType(self):
        return self.Tag_ubyte_box() or self.Tag_uint16_box() or self.Tag_uint32_box() or \
               self.Tag_sint16_box() or self.Tag_sint32_box() or self.Tag_real32_box()
        
    def valueType(self):
	return self.singleValueType() or self.xyValueType() or self.boxValueType()

    def arraySizeType(self):
        return self.Tag_ubyte() or self.Tag_uint16()

    def arraySize(self):
        # save the old unpack string for the type of the array, the data type for the size
        # will replace it.
        unpack_string = self.unpack_string
        size_of_element = self.size_of_element
        if ( self.arraySizeType() ):
            self.size_of_array = self.unpack( self.unpack_string, \
                                         self.data[self.index:self.index+self.size_of_element] )[0]
            print self.size_of_array,
            self.index = self.index + self.size_of_element
            # restore the unpack string
            self.unpack_string = unpack_string
            self.size_of_element = size_of_element
            return 1
        return 0
        
    def singleValueArrayType(self):
        return self.Tag_ubyte_array() or self.Tag_uint16_array() or \
               self.Tag_uint32_array() or self.Tag_sint16_array() or \
               self.Tag_sint32_array() or self.Tag_real32_array()
        
    def arrayType(self):
        if (self.singleValueArrayType() and self.arraySize()):
            array_size = self.size_of_element * self.size_of_array
            array_elements = self.unpack( str(self.size_of_array) + self.unpack_string, \
                                     self.data[self.index:self.index+array_size] )
            for num in array_elements:
                print num,
            print "]",
            self.index = self.index + array_size
            return 1
        return 0

    def dataType(self):
        return( self.valueType() or self.arrayType() or self.boxValueType() )

    # these get parsed when doing the tags
    def numericValue(self):
        return 1;

    def attributeValue(self):
        return( self.dataType() and self.numericValue() )

    def singleAttributePair(self):
        return( self.attributeValue() and self.attributeID() )
    
    def multiAttributeList(self):
        # NB should be many 1+ not sure how this get handled yet
        return( self.singleAttributePair() )
    
    def nullAttributeList(self):
        # NB not sure
        return 0
    
    def attributeList(self):
        return (self.singleAttributePair() or self.multiAttributeList() or self.nullAttributeList())

    def attributeLists(self):
        # save the beginning of the attribute list even if it is
        # empty.  So we can report the position of the command.
        self.begin_attribute_pos = self.index
        # 0 or more attribute lists
        while( self.attributeList() ):
            continue
        return 1

    def UEL(self):
        if ( self.data[self.index:self.index+9] == "\033%-12345X" ):
            print 'string* \\x1B%-12345X'
            self.index = self.index + 9
            return 1
        return 0
        
    def operatorSequences(self):
        while ( self.attributeLists() and self.operatorTag() ) or self.UEL():
            print
            continue
        
    def disassemble(self):
        try:
            self.operatorSequences()
        # assume an index error means we have processed everything - ugly
        except IndexError:
             return
        else:
             print "dissassemble failed"

if __name__ == '__main__':
    import sys

    if not sys.argv[1:]:
        print "Usage: %s pxl files" % sys.argv[0]
        
    for file in sys.argv[1:]:
        try:
            fp = open(file, 'rb')
        except:
            print "Cannot find file %s" % file
            continue
        # read the whole damn thing.  If this get too cumbersome we
        # can switch to string i/o which will use a disk
        pxl_code = fp.read()
        fp.close()
    
        # initialize and disassemble.
        pxl_stream = pxl_dis(pxl_code)
        pxl_stream.disassemble()
