:
trap 'rm $TMP1 $TMP2 $TMP3' 0
TMP1=`mktemp /tmp/tmp.XXXXXXXXXX` || exit 1
TMP2=`mktemp /tmp/tmp.XXXXXXXXXX` || exit 1
TMP3=`mktemp /tmp/tmp.XXXXXXXXXX` || exit 1

cat >$TMP1 <<-EOF
	<!DOCTYPE html>

	<html lang=en>
	<style>
	/* no <elements> or <![CDATA[ mark-up here */
	</style>
EOF
cat >$TMP2 <<-EOF
	<!DOCTYPE html>

	<html lang="en">
	<head>
	<style><![CDATA[
	/* no <elements> or <![CDATA[ mark-up here */
	]]></style></head>
	</html>
EOF
./hxnormalize -i 0 -x $TMP1 | ./hxnormalize -i 0 -x >$TMP3
cmp -s $TMP2 $TMP3
