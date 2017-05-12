## Fridge service writeup

> Author: Andrew Gein, Hackerdom team

Fridge is a service which allow you to register refrigerators, add food to it and save favourite recipes in your account.
Moreover there is an API on port 9595 for retrivieng information about recipes whith contains food from one of your refrigerator.
This API can be used by some kitchen IoT services and gadgets.

All information in this service is private, but flags have been stored in refrigerator descriptions (20% of flags) and
in recipe descriptions (80% of flags).

Fridge service contains 4 docker containers configured with docker-compose: 

* fridge-db for PostgreSQL database
* fridge-django for Django application and uwsgi (sources are in [../services/fridge/web/])
* fridge-nginx for web-server Nginx
* fridge-api for API server (sources are in [../services/fridge/api/])

### Simple vulnerability

First vulnerability allows to stole all refrigerators descriptions. Let's see to file [../services/fridge/config/django.start.sh].
This script should start Django in `fridge-django` docker container. It contains follow lines:

```
 # DON'T RUN IT IN PRODUCTION. SOME EVIL GUYS CAN BRUTEFORCE PASSWORD AND WHO KNOW WHAT HAPPENS...
echo "[+] [DEBUG] Django setup, executing: add superuser"
PGPASSWORD=${POSTGRES_PASSWORD} psql -U ${POSTGRES_USER} -h ${POSTGRES_HOST} -c "INSERT INTO auth_user (password, last_login, is_superuser, username, first_name, last_name, email, is_staff, is_active, date_joined) VALUES ('pbkdf2_sha256\$36000\$k36V24q60mNo\$v5og9qcgc2sqkVwGjZDKNK+wcJy60ix8DIt9E8Yg48c=', '1970-01-01 00:00:00.000000', true, 'admin', 'admin', 'admin', 'admin@admin', true, true, '1970-01-01 00:00:00.000000') ON CONFLICT (username) DO NOTHING"
```

Hmmm. It seems like something strange is added to our database before Django starts. If we will brute password's hash
`pbkdf2_sha256$36000$k36V24q60mNo$v5og9qcgc2sqkVwGjZDKNK+wcJy60ix8DIt9E8Yg48c=` we will be able to login into
django's admin page at `http://<server>:8000/admin/` and view all registered models. As you can see in (admin.py)[../services/fridge/web/]
the only one registerd models is `models.Refrigerator`. So the exploit is easy: send request to `http://<server>:8000/admin/`, authorize
as `admin` with bruteforces password and see all Refrigerator models.

### How to break the hash

Just use hashcat, john the ripper or other tool for hash breaking. The password is dictionary word so breaking was easy even on CPU.

By the way, the password is 'greetings'.

### Hard vulnerability

API on port 9595/tcp have only two commands:

* `LIST <login> <password>` — show all your refrigerators
* `RECIPES <login> <password> <refrigerator_id>` — show all recipes which contains at least one of food which are stored in your refrigerator

The last command is very interesting because it doesn't filter recipes by owner and theoretically can show not yours recipes.
But recipes in output of this command should contain food of some your types (type are just 'potato' or 'milk')
and you should have food of this type in the refrigerator.

So if we are able to add our food type (i.e. our potato or milk) to another's recipe and in our refrigerator, we will see this recipe
(and it's description which contains flag) in the output of the `RECIPES` command.

### Dumper

API has no connection to the database. But how it checks login and password in this case? Web application written on Django dumps all
needed data from database to csv files (see volume 'dumps' in [docker-compose.yml] which is shared between fridge-django and fridge-api containers).

This is an example of such csv file (it's refrigerator_foodtype.csv which are dumped version of `refrigerator.models.FoodType` model):

```
id,owner_id,name,description,unit
1,3,Baked milk,"A variety of boiled milk that has been particularly popular in Russia, Ukraine and Belarus. It is made by simmering milk on low heat for eight hours or longer.",4
2,4,Conchiglie,Seashell shaped,1
```

Dumper (runs)[../services/fridge/web/dumper/middleware.py] dumping on each HTTP request but not other than one time in 5 seconds.

Let's see the (code of dumper)[../service/fridge/web/dumper/__init__.py]. It opens file in UTF-8 and creats csv.DictWriter. You can
read about csv.DictWriter in the official python documentation: [https://docs.python.org/3/library/csv.html#csv.DictWriter].

There is no problem with newlines inside some fields because DictWriter is smart. It will enclose such strings in quotes ("). I.e:

```
id,owner_id,name,description,unit
1,3,Baked milk,"A variety of boiled milk that has been particularly popular in Russia, Ukraine and Belarus.",4
2,4,Conchiglie,Seashell shaped,1
3,5,Capellini,"The thinnest type
of long pasta",3
```

### Dumps loader

API service loads data from these dumps. Obviously it loads with python's standart csv.DictReader: [https://docs.python.org/3/library/csv.html#csv.DictReader].

But there is some bug and unsymmetric behavior in `csv.DictWriter` and `csv.DictReader`.
The most important part of our vulnerability: there are some symbols in Unicode which are line separators besides '\n' and '\r'. I.e.
(LINE SEPARATOR)[http://www.fileformat.info/info/unicode/char/2028/index.htm] or (PARAGRAPH SEPARATOR)[http://www.fileformat.info/info/unicode/char/2029/index.htm].

If you will use one of these symbols in a model's field, `csv.DictWriter` will not think about them as about line separators and will not add quotes.
But csv.DictReader() will see line separators in them and start a new row after it. Lets see on example:

```
id,owner_id,name,description,unit
1,3,Baked milk,"A variety of boiled milk that has been particularly popular in Russia, Ukraine and Belarus.",4
2,4,Conchiglie,Seashell shaped,1
3,5,Capellini,The thinnest type[LINE SEPARATOR CHAR]of long pasta,3
```

`csv.DictWriter` didn't add quotes in description because it doesn't think that \[LINE SEPARATOR CHAR\] is a real line separator.
But `csv.DictReader` see this files differently:

```
id,owner_id,name,description,unit
1,3,Baked milk,"A variety of boiled milk that has been particularly popular in Russia, Ukraine and Belarus.",4
2,4,Conchiglie,Seashell shaped,1
3,5,Capellini,The thinnest type[LINE SEPARATOR CHAR]
of long pasta,3
```

So we can inject some rows and break struture in csv file.

### Exploitation

Let's add a new food type, add it to a refrigerator and create a recipe. After that let's add our food type on the recipe with follow arguments:

```
recipe_id = our recipe's ID
food_type_id = our food type's ID
what_to_do = foobar[LINE SEPARATOR CHAR]10
count = 20
pause_after = our food type's ID
```

When it will dump to csv file, it will turn into

```
id,recipe_id,food_type_id,what_to_do,count,pause_after
1,our recipe's ID,our food type's ID,foobar[LINE SEPARATOR CHAR]10,20,our food type's ID
```

But for `csv.DictReader` this fill will be looked as:

```
id,recipe_id,food_type_id,what_to_do,count,pause_after
1,our recipe's ID,our food type's ID,foobar[LINE SEPARATOR CHAR]
10,20,our food type's ID
```

Oh my God! We added our food type to the recipe with ID = 20. Because it's another's recipe we will add our food to another's recipe. 
Now we should go to API service to port 9595 and send it command `RECIPES <our login> <our password> <our refrigerator id>`. We will see 
recipe #20 and it's description.

Full exploit is available (here)[../sploits/fridge/fridge.exploit.py]
