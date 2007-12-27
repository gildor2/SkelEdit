#!/usr/bin/perl -w

=TODO =========================================================================

- cpp: code to check class field offsets (computed from script, and check with
  FOFS() macro in cpp)
- cpp: code to check native class sizes

=cut  =========================================================================

#------------------------------------------------------------------------------
#	Object flags
#------------------------------------------------------------------------------

# property flags
# should correspond to Object.h PROP_XXX declaratons
sub PROP_EDITABLE	{ return 1;  }
sub PROP_EDITCONST	{ return 2;  }
sub PROP_NOEXPORT	{ return 4;  }
sub PROP_TRANSIENT	{ return 8;  }
sub PROP_NATIVE		{ return 16; }


# prefixes
$CLASS_PREFIX = "C";
$STRUC_PREFIX = "C";


#------------------------------------------------------------------------------
#	Utility functions
#------------------------------------------------------------------------------

# usage: GetTabs(string, resultColumn)
sub GetTabs {
	my ($str, $len) = @_;
	my $i = length($str);
	if ($i >= $len) {
		return " ";
	}
	my $tab = "";
	while ($i < $len)
	{
		$tab .= "\t";
		$i += 4;
	}
	return $tab;
}


#------------------------------------------------------------------------------
#	Simple C++-style lexer
#------------------------------------------------------------------------------

$FILE_NAME = "undefined";
$PARSED_LINE = "";				# initialized as defined!
$COMMENT = "";
$COMMENT_COUNT = 100;			# some large value
$LINE_NO = 0;


$S_RED     = "\033[1;31m";
$S_YELLOW  = "\033[1;33m";
$S_DEFAULT = "\033[0m";


sub ParseError {
	die "${S_RED}${FILE_NAME} : ${LINE_NO} : error : $_[0]${S_DEFAULT}\n";
}

sub ParseWarning {
	print STDERR "${S_YELLOW}${FILE_NAME} : ${LINE_NO} : warning : $_[0]${S_DEFAULT}\n";
}

sub CheckIdent {
	my ($id) = $_[0];
	ParseError("invalid identifier: \"$id\"") if $id !~ /^[A-Za-z_]\w*$/;
}

# usage: Expect(expectedToken, realToken)
# will throw error when incorrect
sub ExpectToken {
	my ($exp, $real) = @_;
	return if $exp eq $real;
	ParseError("unexpected \"$real\" (\"$exp\" expected)");
}


sub NextLine {
	return 0 if ! ($PARSED_LINE = <IN>);
	# line accepted, cleanup
	$LINE_NO++;
	# remove CR/LF
	$PARSED_LINE =~ s/\r//;
	$PARSED_LINE =~ s/\n//;
	# remove C++-style comments
	$PARSED_LINE =~ s/\s*\/\/.*//;
	# remove trailing spaces
	$PARSED_LINE =~ s/\s*$//;
	# remove leading spaces
	$PARSED_LINE =~ s/^\s*//;
}


sub GetToken0 {
	while (1)
	{
		while (!defined($PARSED_LINE) || $PARSED_LINE eq "")
		{
			return 0 if !NextLine();
		}

		# check for /**/ comment
		if ($PARSED_LINE =~ /^\/\*/)
		{
			$COMMENT = "";
			$COMMENT_COUNT = 0;
			# C-style comment
			# remove opening brace
			$PARSED_LINE = substr($PARSED_LINE, 2);
			# read file until closing brace
			my $lines = 0;
			while (1)
			{
				$lines++;
				# remove "* " from "* comment"
				if ($lines > 1)
				{
					$PARSED_LINE =~ s/^\*\s+//;
				}
				if ($PARSED_LINE =~ /\*\//)
				{
					# line contains closing brace
					my $comment;
					($comment, $PARSED_LINE) = $PARSED_LINE =~ /^(.*)\*\/\s*(.*)/;
					$COMMENT .= $comment;
					last;
				}
				if ($COMMENT) {
					$COMMENT = $COMMENT."\n".$PARSED_LINE;
				} else {
					$COMMENT = $PARSED_LINE;
				}
				NextLine() or ParseError("unexpected EOF: parsing /**/ comment");
			}
			# remove trailing spaces
			$COMMENT =~ s/\s*$//;
#			print "PARSED COMMENT:\n{\n$COMMENT\n}\n";
		}
		else
		{
			return 1;
		}
	}
}


# return next token or "undef" if end of file reached
# usage:
#	GetTok()		get token; when impossible - throw error
#	GetTok(1)		get token; when impossible - return "undef"
sub GetToken {
	my $noerr = $_[0];
	if (!GetToken0())
	{
		if (!defined($noerr) || !$noerr) {
			ParseError("unexpected end of file");
		} else {
			return undef;
		}
	}

	$COMMENT_COUNT++;

	my ($restLine, $token);
	if ($PARSED_LINE =~ /^\"/)
	{
		# string
		($token, $restLine) = $PARSED_LINE =~ /^(\".*\")\s*(.*)/;
		ParseError("unexpected EOLN: parsing string") if !defined($token);
	}
	else
	{
		($token, $restLine) = $PARSED_LINE =~ /^(\-?[\w\.]+)\s*(.*)/;
	}
	# cannot get token - return a single char
	if (!defined($token))
	{
		# not a word
		($token, $restLine) = $PARSED_LINE =~ /^(.)\s*(.*)/;
	}
	$PARSED_LINE = $restLine;

	return $token;
}


sub ForceEmptyRestLine {
	ParseError("extra characters in line") if $PARSED_LINE ne "";
}


sub GetNextLine {
	my $line = <IN>;
	ParseError("unexpected end of file") if !defined($line);
	$LINE_NO++;
	# remove CR/LF
	$line =~ s/\r//;
	$line =~ s/\n//;
	return $line;
}


#------------------------------------------------------------------------------
#	Type registry
#------------------------------------------------------------------------------

#	Type info:
#	----------
#	script name
#	native name
#	type kind: scalar, enum, struct, class

sub TYPE_SCALAR { return 0; }
sub TYPE_ENUM   { return 1; }
sub TYPE_STRUCT { return 2; }
sub TYPE_CLASS  { return 3; }

%types = ();

# usage:
#	RegisterType(ScriptName, NativeName, TypeKind)
sub RegisterType {
	my ($scName, $natName, $kind) = @_;
	# check type existance
	my $typeInfo = $types{$scName};
	if ($typeInfo)
	{
		ParseError("type \"$scName\" is already defined in ".$typeInfo->{source});
	}
	CheckIdent($scName);
	# register new type
	my $rec =
	{
		natName		=> $natName,
		kind		=> $kind,
		source		=> "$FILE_NAME : $LINE_NO"
	};
	$types{$scName} = $rec;
}


# usage:
#	GetTypeinfo(name)		return type info or throw error when type not found
#	GetTypeinfo(name, 1)	return type info or "undef" when not found
sub GetTypeinfo {
	my ($name, $noerr) = @_;
	my $rec = $types{$name};
	if (!defined($rec) && (!defined($noerr) || !$noerr)) {
		ParseError("type \"$name\" is not defined");
	}
	return $rec;
}


# register native types
RegisterType("bool",   "bool",     TYPE_SCALAR);
RegisterType("byte",   "byte",     TYPE_SCALAR);
RegisterType("short",  "short",    TYPE_SCALAR);
RegisterType("ushort", "word",     TYPE_SCALAR);
RegisterType("int",    "int",      TYPE_SCALAR);
RegisterType("uint",   "unsigned", TYPE_SCALAR);
RegisterType("float",  "float",    TYPE_SCALAR);


#------------------------------------------------------------------------------
#	Typeinfo file support
#------------------------------------------------------------------------------

sub WriteCompactIndex {
	my ($idx) = $_[0];
#printf("Write %d (0x%X)\n", $idx, $idx);
	my $b = 0;
	# generate sign bit
	if ($idx < 0)
	{
		$idx = -$idx;
		$b |= 0x80;
#printf("neg, value= %d (0x%X)\n", $idx, $idx);
	}
	$b |= $idx & 0x3F;
	# write 1st byte
	if ($idx <= 0x3F)
	{
#printf("B=%02X\n", $b);
		syswrite(TYPE, pack("C", $b));
		return;
	}
	$b |= 0x40;
#printf("B=%02X\n", $b);
	syswrite(TYPE, pack("C", $b));
	$idx >>= 6;
	die "UNEXPECTED ERROR" if !$idx;
	# write other bytes
	while ($idx)
	{
		$b = $idx & 0x7F;
		$idx >>= 7;
		if ($idx)
		{
			$b |= 0x80;
		}
#printf("B=%02X\n", $b);
		syswrite(TYPE, pack("C", $b));
	}
}


sub WriteBinString {
	my ($str) = $_[0];
	syswrite(TYPE, $str);
	syswrite(TYPE, pack("C", 0));
}


sub WriteBinDword {
	my ($dw) = $_[0];
	syswrite(TYPE, pack("l", $dw));
}


sub WriteBinTypeHdr {
	my ($name, $type) = @_;
	WriteBinString($name);
	WriteCompactIndex($type);
}


#------------------------------------------------------------------------------
#	Script parser
#------------------------------------------------------------------------------

$PASS = 0;
$NOCPP = 0;

$CLASS_NAME    = "";
$CLASS_PARENT  = "";
$CLASS_COMMENT = "";

sub GetComment {
	my $cmt = $COMMENT;
	$COMMENT = "";
	if ($COMMENT_COUNT != 1 || $cmt !~ /^\*/) {
		# no comment, or non-doxygen comment
		$cmt = "" ;
	} else {
		$cmt =~ s/^\*\s*//;
	}
	return $cmt;
}


sub DumpComment {
	my ($cmt, $tab) = @_;
	if ($cmt)
	{
		print CPP "$tab/**\n";
		my @lines = split('\n', $cmt);
		for my $line (@lines) {
			print CPP "$tab * $line\n";
		}
		print CPP "$tab */\n";
	}
}


# returns an array of tokens until trailing ';'
sub GetScriptLine {
	my @TOKENS = ();
	while (my $token = GetToken(1))
	{
		return @TOKENS if $token eq ";";
		push @TOKENS, $token;
	}
	ParseError("unexpected EOF: ';' expected");
}

%defines = ();

sub GetNumber {
	my $num = $_[0];
	return $num if $num =~ /^(\-)?[\d\.]+$/;
	if (exists($defines{$num})) {
		return $defines{$num};
	}
	ParseError("invalid number: $num");
}


sub ParseClassHeader {
	# read script header
	my $token = GetToken();
	ExpectToken("class", $token);
	$CLASS_COMMENT = GetComment();
	my @HEADER = GetScriptLine();
	ParseError("invalid class declaration") if !@HEADER;

	$CLASS_NAME = $HEADER[0];
	$CLASS_PARENT = "";
	$NOCPP = 0;
	# parse class attributes
	for (my $i = 1; $i <= $#HEADER; $i++)
	{
		my $attrib = $HEADER[$i];
		if ($attrib eq "extends") {
			$i++;
			$CLASS_PARENT = $HEADER[$i];
			CheckIdent($CLASS_PARENT);
		} elsif ($attrib eq "noexport") {
			$NOCPP = 1;
		} else {
			ParseError("unknown class attribute: \"$attrib\"");
		}
	}
}


# usage ParseField(ShouldDump)
sub ParseField {
	my ($process) = @_;

	local @DESCR;		# "local" for NextTok()
	my $tok;

	# helper function
	# usage:
	#	NextTok()		get token; when impossible - throw error
	#	NextTok(1)		get token; when impossible - return "undef"
	sub NextTok {
		my $noerr = $_[0];
		my $tok = shift @DESCR;
		if (!defined($tok) && (!defined($noerr) || !$noerr)) {
			ParseError("enexpected end of line while parsing property");
		}
		return $tok;
	}

	# gather property information into following variables:
	#	HELP			property description for editor
	#	FLAGS			set of PROP_XXX flags
	#	EDITGROUP		visualization group for editor

	my $HELP = GetComment();
	# read @DESCR after taking comment
	@DESCR = GetScriptLine();
	ParseError("invalid variable declaration") if !@DESCR;

	# get editable flags
	my $FLAGS = 0;
	my $EDITGROUP = "";

	# check for editor group
	$tok = NextTok();
	if ($tok eq "(")
	{
		# var(group) syntax
		$FLAGS |= PROP_EDITABLE;
		while (1)
		{
			$tok = NextTok();
			last if $tok eq ")";
			if (!$EDITGROUP) {
				$EDITGROUP = $tok;
			} else {
				$EDITGROUP .= " $tok";
			}
		}
		# read next token
		$tok = NextTok();
	}

	# process property flags
	while (1)
	{
		if ($tok eq "editconst") {
			$FLAGS |= PROP_EDITCONST;
		} elsif ($tok eq "noexport") {
			$FLAGS |= PROP_NOEXPORT;
		} elsif ($tok eq "transient") {
			$FLAGS |= PROP_TRANSIENT;
		} elsif ($tok eq "native") {
			$FLAGS |= PROP_NATIVE;
		} else {
			last;
		}
		# read next token
		$tok = NextTok();
	}

	# now, should parse property type and name
	# variants:
	#	var ... type name
	#	var ... array<type> name
	#	var ... type name[size]
	#	var ... pointer name{type}
	#	var ... name1, name2, name3
	my $type = $tok;
	my $natType;
	my $strSize = 0;
	my $dynArray = 0;
	if ($type eq "string") {
		# string[len]
		ExpectToken("[", NextTok());
		my $sizeS = NextTok();
		$strSize = GetNumber($sizeS);
		ExpectToken("]", NextTok());
		$natType = "TString<$sizeS>";
	} elsif ($type eq "array") {
		# array<type>
		$dynArray = 1;
		ExpectToken("<", NextTok());
		$type = NextTok();
		ExpectToken(">", NextTok());
		CheckIdent($type);
		my $typeInfo = GetTypeinfo($type);
		$natType = "TArray<".$typeInfo->{natName}.">";
	} else {
		# regular type
		CheckIdent($type);
		my $typeInfo = GetTypeinfo($type);
		$natType = $typeInfo->{natName};
	}
	# compute fine tabulations for C++ type-name declaration
	my $typePad = GetTabs($natType, 28);
	# parse variable name(s)
	while (1)
	{
		my $name = NextTok();
		CheckIdent($name);	#?? check for name duplicate
		my $size  = 0;
		my $sizeS = "";
		# check for extra tokens
		my $sep = NextTok(1);
		if (defined($sep) && $sep eq "[") {
			ParseError("string arrays are unsupported") if $strSize;
			ParseError("arrays of arrays are unsupported") if $dynArray;
			# static array declaration
			$sizeS = NextTok();
			$size = GetNumber($sizeS);
			ParseError("invalid array size \"$size\"") if $size < 1;
			ExpectToken("]", NextTok());
			$sep = NextTok(1);
		}
		# generate code
		if ($process)
		{
			# register field
			if (!($FLAGS & PROP_NOEXPORT) && !$NOCPP)
			{
				# create C++ field declaration
				DumpComment($HELP, "\t");
				print CPP "\t${natType}${typePad}${name}";
				print CPP "[$sizeS]" if $size > 0;
				print CPP ";\n";
			}
			# generate typeinfo
			WriteBinString($name);				# field name
			WriteBinString($type);				# field type
			if ($strSize) {
				WriteCompactIndex($strSize);	# string length
			} elsif ($dynArray) {
				WriteCompactIndex(-1);			# dynamic array marker
			} else {
				WriteCompactIndex($size);		# array size (0 for scalars)
			}
			WriteBinDword($FLAGS);				# field flags
			WriteBinString($HELP);				# comment
			WriteBinString($EDITGROUP);			# editor group
#			print CPP "\t// ($FLAGS : \"$EDITGROUP\")\n";
		}
		# check for multiple variables in a single line declaration
		last if !defined($sep);
		ExpectToken(",", $sep);
	}
}


sub ParseEnum {
	if ($PASS > 1)
	{
		# simply skip enum declaration
		GetScriptLine();
		return;
	}
	# parse unum type
	my $enumName = GetToken();
	CheckIdent($enumName);
	my $sep = GetToken();
	ExpectToken("{", $sep);

	# register new enum type
	print CPP "enum $enumName\n{\n" if !$NOCPP;
	RegisterType($enumName, $enumName, TYPE_ENUM);
	WriteBinTypeHdr($enumName, TYPE_ENUM);

	while (1)
	{
		my $name = GetToken();
		last if $name eq "}";		# allow 'enum { value1, value2 ... , }' (',' before '}')
		CheckIdent($name);
		# add enum value
		print CPP "\t$name,\n" if !$NOCPP;
		WriteBinString($name);
		# check separator
		$sep = GetToken();
		last if $sep eq "}";
		ExpectToken(",", $sep);
	}
	print CPP "};\n\n" if !$NOCPP;
	WriteBinString("");				# "end of declaration" marker
	$sep = GetToken();
	ExpectToken(";", $sep);
}


# usage ParseCppText(ShouldDump)
sub ParseCppText {
	my ($process) = $_[0];
	my $sep = GetToken();
	ExpectToken("{", $sep);
	ForceEmptyRestLine();
	my $level = 1;
	my $tabRemove = -1;
	my $dump = $process && !$NOCPP;

	if ($dump) {
		print CPP "\n";
	}
	while (1)
	{
		my $line = GetNextLine();
		# count number of opening and closing braces in line
		my $left  = $line =~ s/{/{/g;
		my $right = $line =~ s/}/}/g;
		# update and analyze current brace level
		$level = $level + $left - $right;
		ParseError("mismatched {}") if $level < 0;
		if ($level == 0)
		{
			# allow closing "cpptext" brace to be only on separate line
			ParseError("extra characters in line with closing \"}\"") if $line !~ /^\s*}\s*$/;
			# end of block
			last;
		}

		if ($process && !$NOCPP) {
			if ($tabRemove == -1) {
				if ($line =~ /^\t\t/) {
					$tabRemove = 1;
				} else {
					$tabRemove = 0;
				}
			}
			if ($tabRemove) {
				$line =~ s/^\t//;
			}
			print CPP "$line\n";
		}
	}
}


sub ParseStruct {
	my $HELP = GetComment();
	my $strucName = GetToken();
	my $parent = "";
	my $process = $PASS == 1;
	CheckIdent($strucName);
	my $sep = GetToken();
	if ($sep eq "extends")
	{
		# parse parent structure
		$parent = GetToken();
		my $typeInfo = GetTypeinfo($parent);
		ParseError("cannot derive structure type from non-structure") if $typeInfo->{kind} != TYPE_STRUCT;
		$sep = GetToken();
	}
	ExpectToken("{", $sep);
	my $natName = $STRUC_PREFIX.$strucName;
	if ($process)
	{
		if (!$NOCPP)
		{
			print CPP "\n";
			DumpComment($HELP, "");
			print CPP "struct $natName";
			print CPP " : ${STRUC_PREFIX}${parent}" if $parent;
			print CPP "\n{\n";
		}
		WriteBinTypeHdr($strucName, TYPE_STRUCT);
		WriteBinString($parent);
	}
	# parse structure fields
	while (1)
	{
		$sep = GetToken();
		last if $sep eq "}";
		if ($sep eq "structcpptext")
		{
			ParseCppText($process);
			next;
		}
		ExpectToken("var", $sep);
		ParseField($PASS == 1);
	}
	if ($process)
	{
		RegisterType($strucName, $natName, TYPE_STRUCT);
		WriteBinString("");			# "end of declaration" marker
		print CPP "};\n\n" if !$NOCPP;
	}
	$sep = GetToken();
	ExpectToken(";", $sep);			#?? also can create enumeration variables here
}


sub ParseConst{
	my ($process) = @_;
	# "const name value;"
	my $name = GetToken();
	CheckIdent($name);
	ExpectToken("=", GetToken());
	my $value = GetNumber(GetToken());
	ExpectToken(";", GetToken());

	if ($process) {
		# cpp declaration
		my $tabs = GetTabs($name, 24);
		print CPP "#define ${name}${tabs}${value}\n";
		# register constant
		ParseError("name \"$name\" is already defined") if exists($defines{$name});
		$defines{$name} = $value;
	}
}


sub ParseFilePass {
	open(IN, $FILE_NAME) or die "Unable to open input file $FILE_NAME\n";

	my $ExportClass = $PASS == 2;

	ParseClassHeader();
	# verify: filename should be the same, as declared class inside file (case-sensitive)
	if ("$CLASS_NAME.uc" ne "$FILE_NAME") {
		ParseError("class \"$CLASS_NAME\" not corresponding to file name $FILE_NAME");
	}
	#!! + add "DependsOn" keyword

	my $CppName = "${CLASS_PREFIX}${CLASS_NAME}";
	if ($CLASS_PARENT)
	{
		my $type = GetTypeinfo($CLASS_PARENT, 1);
		if ($type)
		{
			if ($type->{kind} != TYPE_CLASS)
			{
				ParseError("\"$CLASS_PARENT\" is not a class type (declared in )".$type->{source});
			}
		}
		else
		{
			# should parse parent class file before this ...
			return $CLASS_PARENT;
		}
	}
	if ($ExportClass) {
		RegisterType($CLASS_NAME, $CppName, TYPE_CLASS);
		WriteBinTypeHdr($CLASS_NAME, TYPE_CLASS);
		WriteBinString($CLASS_PARENT);
	}

	if ($PASS == 1 && !$NOCPP)
	{
		print CPP "/*------------------------------------------------------------------------------\n";
		print CPP "\t$CppName class (declared in $FILE_NAME)\n";
		print CPP "------------------------------------------------------------------------------*/\n\n";
	}

	if ($ExportClass && !$NOCPP)
	{
		print CPP "\n";
		# dump class header
		DumpComment($CLASS_COMMENT, "");
		print CPP "class $CppName";
		if ($CLASS_PARENT) {
			print CPP " : public ${CLASS_PREFIX}${CLASS_PARENT}\n";
			print CPP "{\n\tDECLARE_CLASS($CppName, ${CLASS_PREFIX}${CLASS_PARENT})\n";
		} else {
			print CPP "\n{\n\tDECLARE_BASE_CLASS($CppName)\n";
		}
		print CPP "public:\n"
	}

	while (my $token = GetToken(1))
	{
		if ($token eq "var") {
			ParseField($ExportClass);
		} elsif ($token eq "enum") {
			ParseEnum();
		} elsif ($token eq "struct") {
			ParseStruct();
#		} elsif ($token eq "defaultproperties") {
#!!			ParseDefaultProps();
		} elsif ($token eq "cpptext") {
			ParseCppText($ExportClass);
		} elsif ($token eq "const") {
			ParseConst($PASS == 1 && !$NOCPP);
		} else {
			ParseError("unexpected token \"$token\"");
		}
	}

	if ($ExportClass)
	{
		# close class declaration
		print CPP "};\n\n\n" if !$NOCPP;
		WriteBinString("");		# "end of declaration" marker
	}

	close(IN);

	return undef;
}


%parsedFiles = ();

# usage:
#	ParseFile(filename)			parse file and throw error, if already parsed (catch recursion)
#	ParseFile(filename, 1)		no error, when file already parsed
sub ParseFile {
	my ($name, $checkCompiled) = @_;

	if (exists($parsedFiles{$name})) {
		return if $checkCompiled;
		ParseError("class recursion: $name");
	}
#	print STDERR "*** parse $name\n";
	$parsedFiles{$name} = 1;

	$FILE_NAME = $name;
	$LINE_NO   = 0;
	$PASS      = 1;
	my $parent = ParseFilePass();
	if ($parent)
	{
		# should parse parent file ...
#		print STDERR "*** delayed, needs $parent\n";
		my $pfile = "$parent.uc";
		if (! -f $pfile) {
			ParseError("file $pfile is not found");
		}
		ParseFile($pfile);
		# ... then this file again
#		print STDERR "*** parse $name again\n";
		$FILE_NAME = $name;
		$LINE_NO   = 0;
		$PASS      = 1;
		ParseFilePass();
	}
	$LINE_NO   = 0;
	$PASS      = 2;
	ParseFilePass();
}


open(CPP, ">SkelTypes.h") or die "Cannot create header file";		#!! specify from command line
open(TYPE, ">typeinfo.bin") or die "Cannot create typeinfo file";
binmode(TYPE);


print CPP <<EOF
/*==============================================================================
	C++ class definitions exported from script.
	This is automatically generated by the tools.
	DO NOT modify this manually! Edit the corresponding .uc files instead!
==============================================================================*/


EOF
;

opendir(DIR, ".");
@filelist = readdir(DIR);
closedir(DIR);
# case-insensitive sort of files (for predictable behaviour)
@filelist = sort {uc($a) cmp uc($b)} @filelist;

for $f (@filelist) {
	if (-f $f && $f =~ /\.uc$/) {
		ParseFile($f, 1);
	}
}

close(CPP);
WriteBinString("");			# end of type info marker (empty type name)
close(TYPE);
