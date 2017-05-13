## Doorlock service writeup

> Author: Dimmoborgir, Hackerdom team

Сервис представляет собой контроллер "умных замков". Обрабатывает запросы по
протоколу COAP на порту 5683/udp. Протокол COAP похож на HTTP, только бинарный 
и через UDP :) В сервисе можно регистрировать замки и "прописывать"
в эти замки карточки доступа. При регистрации нового замка сервис генерирует
случайный идентификатор. Для добавления карточки нужно указать Id замка и Tag
карточки. Tag карточки является флагом. Данные хранятся в базе данных LDAP.

Запросы к сервису удобно выполнять из командной строки клиентом на nodejs:

```
$ sudo apt install nodejs npm
$ sudo npm install coap-cli -g
$ coap get "coap://10.60.123.2/get_card?lock=QWERTY&card=123456"
(2.05)	EMPTY
```

## First look

В каталоге сервиса (/home/ctf/doorlock) видим единственный файл docker-compose.yml.
Ни исходников, ни исполняемых файлов сервиса здесь нет.

Заглянем в запущенный docker-контейнер:

```
root@phdays2017:~# docker exec -i -u root -t doorlock /bin/bash

root@7119bb47044d:/app# ps axf
  PID TTY      STAT   TIME COMMAND
   42 ?        Ss     0:00 /bin/bash
   72 ?        R+     0:00  \_ ps axf
    1 ?        Ss     0:00 /bin/sh -c /app/docker-wrapper.sh
    5 ?        S      0:00 /bin/bash /app/docker-wrapper.sh
    6 ?        Sl     0:00  \_ /usr/sbin/slapd -h ldap://127.0.0.1:389/ ...
   32 ?        S      0:00  \_ /app/doorlock-server
```
Видим два главных процесса: slapd (LDAP сервер) и doorlock-server (сервис)

В каталоге /app есть исходный код сервиса (doorlock-server.cpp):
```
root@7119bb47044d:/app# ls
Makefile        docker-wrapper.sh  doorlock-server.cpp  doorlock-server.o  ldap-dpkg-reconfigure.sh 
ldap.cfg        add-locks.ldif     doorlock-server      doorlock-server.d  include
ldap-init.sh    libs
```
Скопируем каталог /app из контейнера на игровой образ для изучения:
```
root@phdays2017:~# docker cp doorlock:/app app
```
## Vuln 1 (simple)

Посмотрим doorlock-server.cpp. Функция init_resources регистрирует обработчики 
COAP-запросов. Сервис отвечает на запросы:

```
GET  /              без параметров
```
Всегда отвечает одинаковой строкий. Не содержит работы с LDAP - нам не интересен.

```
POST /register_lock с параметрами model, floor, room
```
Создает в LDAP объект "замок" (lockObject) структуры:
```
    objectClass: top
    objectClass: device
    objectClass: lockObject
    lockModel:   [параметр model запроса]
    lockFloor:   [параметр floor запроса]
    lockRoom:    [параметр room запроса]
    timestamp:   199412161032Z
```
Объект создается с DN: cn=[lock_id],cn=locks,dc=iot,dc=phdays,dc=com
(DN в LDAP уникально описывает объект в дереве объектов)
[lock_id] - случайная строка из 9 символов, генерируемая сервисом
```
POST /add_card      с параметрами lock, card, tag
```
Создает в LDAP объект "карта доступа" (cardObject) следующей структуры:
```
    objectClass: top
    objectClass: device
    objectClass: cardObject
    lockId:      [параметр lock запроса]
    card:        [параметр card запроса]
    cardTag:     [параметр tag запроса]
    cardOwner:   John Doe
    timestamp:   199412161032Z
```
Объект создается с DN: cn=[card],cn=[lock],cn=locks,dc=iot,dc=phdays,dc=com
```
GET  /get_card      с параметрами lock, card
```
Делает запрос к LDAP на чтение. Запрос строится так: `sprintf( query, "(&(cn=%s)(lockId=%s))", card, lock )`

Синтаксис `(&(FOO)(BAR))` означает одновременное выполнение условий FOO и BAR.

После построения запроса он прогоняется через функции:
1. ldap_sanitize() - делает две вещи
  * заменяет "опасные" символы (*,~,\\,|) на _ 
  * ставит нулевой символ после последней закрывающей скобки
2. ldap_validate() - проверяет что в строке ровно 3 открывающих и ровно 3 закрывающих скобки

В этих функциях кроется первая уязвимость: если передать вместо card строку вида
`N)(%26))`, это приведет к формированию запроса `(&(cn=X)(&))`.
`(&)` всегда является истиной. Таким образом, остаточно знать card_id, чтобы узнать
ее секретный tag (флаг).

Заметим, что в сервисе регистрируются карты с идентификаторами двух видов:
1. secur???????? (где ? - любой символ) - таких 80%
2. N (где N - натуральное число, по возрастанию, начиная с 1) - таких 20%

Первая уязвимость позволяет отправкой запросов `/get_card` с параметром card: 
"1)(%26))", "2)(%26))", "3)(%26))", "4)(%26))", ... получить 20% флагов.
Параметр lock при этом может быть любым - он будет проигнорирован.

## Vuln 1 (hard)

Сервис использует реализацию протокола COAP в библиотеке libcoap, которая лежит
в каталоге libs. Посмотрим, какие в ней есть строки:

```
root@phdays2017:~# strings libcoap-1.a
```

Видим подозрительные строки, наталкивающие нас на мысль о бэкдоре:

```
commit 012a1a1482e30de1dda7d142de27de91d23c4501Merge: b425b15 f122811
Author: Krait <krait@dqteam.org>
Date:   Wed May 3 14:45:52 2017 +0500
Merge branch 'backdoor'
```
```
/home/dima/git/phdctf-2017/services/doorlock/backdoor/libcoap
```
В первой строке есть хэш коммита b425b15, погуглив его находим репозиторий https://github.com/obgm/libcoap

Клонируем себе, переключаемся на коммит b425b15 и собираем:

```
root@phdays2017:~# git clone https://github.com/obgm/libcoap.git
root@phdays2017:~# cd libcoap/
root@phdays2017:~/libcoap# git checkout b425b15
root@phdays2017:~/libcoap# apt-get -y install libtool autoconf pkgconf
root@phdays2017:~/libcoap# ./autogen.sh && ./configure --disable-examples && make
```
Сравним файлы `libcoap-1.a`: только что собранный и из каталога сервиса.
Воспользуемся командой: `objdump -M intel -d libcoap-1.a`
Обернем ее в простой скрипт для сравнения функций (./objd-lines.pl)

```
#!/usr/bin/perl
# objd-lines.pl

@ARGV==2 or die "Args: ORIG PATCHED\n";

sub counts {
	my $fname = shift;
	my %R = ();
	my ($name, $lines) = ('', 0);
	for (`objdump -M intel -d $fname`) {
		if (m|^\S+ <(\S+)>:|) {
			$R{$name} = $lines if $name;
			($name, $lines) = ($1, 0);
		}
		$lines++ if m|^\s+\S+:|;
	}
	$R{$name} = $lines if $name;
	return %R;
}

my %O  = counts($ARGV[0]);
my %M = counts($ARGV[1]);

for (sort {$a->[1] <=> $b->[1]} map { [$_, $M{$_}-$O{$_} ] } keys %M) {
	my $k = $_->[0];
	my $delta = $_->[1];
	next if $delta == 0 || $M{$k} == 0 || $O{$k} == 0;
	printf "%30s   orig:%4d   mod:%4d   diff:%2d\n", $k, $O{$k}, $M{$k}, $M{$k} - $O{$k};
}
```

```
# ./objd-lines.pl .libs/libcoap-1.a ../app/libs/libcoap-1.a 
                  write_option   orig: 140   mod: 134   diff:-6
               coap_retransmit   orig:  82   mod:  79   diff:-3
    print_readable.constprop.1   orig:  87   mod:  85   diff:-2
          coap_write_block_opt   orig: 125   mod: 123   diff:-2
                   no_response   orig:  70   mod:  69   diff:-1
          coap_print_wellknown   orig: 256   mod: 257   diff: 1
                 coap_add_data   orig:  68   mod:  69   diff: 1
                coap_add_block   orig:  19   mod:  21   diff: 2
               coap_print_link   orig: 208   mod: 211   diff: 3
                coap_pdu_parse   orig: 178   mod: 181   diff: 3
         coap_add_option_later   orig:  63   mod:  67   diff: 4
               coap_add_option   orig:  55   mod:  60   diff: 5
       coap_wellknown_response   orig: 292   mod: 299   diff: 7
                  hash_segment   orig:   4   mod:  11   diff: 7
             coap_network_send   orig: 130   mod: 170   diff:40
```
Функция `coap_network_send` увеличилась больше всех - на 40 опкодов.
Посмотрим на дизассемблированный код этой функции внимательнее.

Добавилось два блока.

В первом блоке происходит копирование отправляемого пакета целиком в буфер.

Во втором блоке идет поиск в отправляемом пакете сигнатуры 'LITLGIRL':
```
 496:	48 be 4c 49 54 4c 47 	movabs rsi,0x4c5249474c54494c
```
```
# python3 -c "print(bytearray.fromhex('4c5249474c54494c'))"
bytearray(b'LRIGLTIL')
```
Если сигнатура найдена, отправляется содержимое сохраненного ранее буфера.

Бэкдор используется следующим образом:
* Сохраняем в сервис флаг, начинающийся на 'LITLGIRL', например: `LITLGIRLJTINJEKCADGGJBWDEIOVCMX=`
* Получаем этот флаг. В ответ придет не наш флаг, а флаг другого пользователя, поставленный последним.

Эксплоиты находятся в каталоге 'sploits/doorlock/'

