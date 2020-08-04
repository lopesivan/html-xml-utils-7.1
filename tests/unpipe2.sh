:
trap 'rm $TMP1 $TMP2' 0
TMP1=`mktemp /tmp/tmp.XXXXXXXXXX` || exit 1
TMP2=`mktemp /tmp/tmp.XXXXXXXXXX` || exit 1

cat >$TMP1 <<-EOF
	<!DOCTYPE root>
	<!--
	  A comment
	-->
	<root>
	<a foo="bar&#1000;bar" x="123 456"><b /></a>
	<?pi?>
	Text
	</root>
EOF
./hxpipe $TMP1 | ./hxunpipe >$TMP2

cmp -s $TMP1 $TMP2
