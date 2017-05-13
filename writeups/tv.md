## TV service writeup

TV service allows you to create users, upload images and view them. For each image you can add text comments.

Service converts each uploaded image to png format and resizes it to 128x128 using `convert` util from `imagemagick` package.

Docker container `tv` has an old version of imagemagick installed. It's vulnerable to [CVE-2016-3714](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2016-3714). See detailes at https://imagetragick.com/. Tv uses modified version of imagemagick's config files `police.xml` and `delegates.xml`, which are protected from this vulnerability. So we should modify this file before imagetragick exploitation.

There is second vulnerability which allows to upload files to any folder on the server. So if you upload an empty file with name '../im/test.png/../delegates.xml' you will rewrite original `delegates.xml` and open it to imagetragick.

After all we should just use exploit for imagetragick. See example in the [sploit](../sploits/tv/tv.pl).
