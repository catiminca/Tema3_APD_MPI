Minca Ecaterina - Ioana 334CAb

Am decis sa fac aceasta tema in C++ din lejeritatea pe care o ofera in 
utilizarea structurilor de date. De asemenea, am folosit std::threads in loc
de pthreads.

Am avut de implementat o simulare a protocolul Bit Torrent, folosind MPI.
Astfel, programul se impartea in procese: tracker-ul(0) si clientii.

La inceput, tracker-ul era informat de datele primite de la fiecare client,
despre ce fisiere detine, acesta trimitand ulterior catre fiecare un
mesaj de "ok" prin care ii spunea astfel clientului ca se poate incepe procesul
de obtinere a fisierelor pe care fiecare le doreste. Apoi fiecare client ii
trimite tracker-ului ce fisiere si-ar dori, iar acesta le retine intr-un set
pentru a putea vedea ulterior ca toti clientii au toate fisierele dorite si
se poate inchide programul. Pentru fiecare informatie pe care vor sa o
trasmita clientii catre tracker, se va trimite inainte un mesaj de recunoastere
a tipului de operatie care urmeaza sa se execute. Astfel, un client poate sa ii
trimite un fisier pe care acesta il doreste si sa primeasca informatii despre el,
ii spune acestuia ca a terminat de descarcat un fisier, si-a terminat toate
fisierele sau are o actualizare despre noi segemente pe care le detine.

La client, am decis pentru performanta sa se aleaga random urmatorul client,
nu sa fie unul prestabilit. Pentru "trimiterea" hash-urilor intre clienti, am
ales sa trimita clientul actual hash-ul, iar celalalt va da "ack" doar daca
detine acel fisier(verificand iar acest lucru in functia de upload). Am ales sa
folosesc pentru thread-uri 2 variabile globale, avand ambele acces la acestea
fiind memoria partajata, un map pentru a retine corespondenta client-file-hash uri
si una cu care verific ca numarul de hash-uri pe care fisierul descarcat le are
este cel bun. Si pentru tracker am folosit map-uri pentru a retine ce fisiere are
fiecare client, cat si fiecare fisier ce clienti il detin si la fel si la segmente
(Am o placere vinovata pentru unordered_map don't judge:))

