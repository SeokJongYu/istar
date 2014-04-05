istar
=====

[istar] is a software-as-a-service platform for bioinformatics and chemoinformatics.


Architecture
------------

![istar architecture](https://github.com/HongjianLi/istar/raw/master/public/architecture.png)


Components
----------

### Web client

* [Twitter Bootstrap]
* [jQuery]
* [jQuery UI]
* [three.js]
* [zlib.js]
* [jquery-dateFormat]
* [jquery_lazyload]

### Web server

* [node.js]
* [mongodb]
* [express]
* [spdy]

### Database

* [MongoDB]


Supported browsers
------------------

* Google Chrome 30
* Mozilla Firefox 25
* Microsoft Internet Explorer 11
* Apple Safari 6.1
* Opera 17


REST API for idock
------------------

### Submit a new job via HTTP POST

    curl -d $'receptor=
    ATOM      1  N   PRO A  19     148.930 114.148   5.178  1.00138.31           N  \n
    ATOM      2  CA  PRO A  19     149.869 115.263   5.360  1.00138.99           C  \n
    TER    2743      GLN A 313                                                      \n
    &center_x=150&center_y=109&center_z=22&size_x=18&size_y=17&size_z=15
    &description=4MBS&email=Jacky@cuhk.edu.hk
    &mwt_lb=400&mwt_ub=420&lgp_lb=0&lgp_ub=2&nrb_lb=4&nrb_ub=6
    &hbd_lb=2&hbd_ub=4&hba_lb=4&hba_ub=6&chg_lb=0&chg_ub=0
    &ads_lb=0&ads_ub=5&pds_lb=-20&pds_ub=0&psa_lb=60&psa_ub=80'
    http://istar.cse.cuhk.edu.hk/idock/jobs

### Obtain existing jobs via HTTP GET

    curl http://istar.cse.cuhk.edu.hk/idock/jobs

### Count the number of ligands satisfying your custom filtering conditions via HTTP GET

    curl -Gd
    'mwt_lb=400&mwt_ub=500&lgp_lb=0&lgp_ub=5&nrb_lb=2&nrb_ub=8&
    hbd_lb=2&hbd_ub=5&hba_lb=2&hba_ub=10&chg_lb=0&chg_ub=0&
    ads_lb=0&ads_ub=12&pds_lb=-50&pds_ub=0&psa_lb=20&psa_ub=100'
    http://istar.cse.cuhk.edu.hk/idock/ligands


REST API for igrow
------------------

### Submit a new job via HTTP POST

    curl -d 'idock_id=525a0abab0717fe31a000001&description=4MBS&email=Jacky@cuhk.edu.hk&mwt_lb=300&mwt_ub=500&nrb_lb=4&nrb_ub=6&hbd_lb=2&hbd_ub=4&hba_lb=4&hba_ub=6' http://istar.cse.cuhk.edu.hk/igrow/jobs

### Obtain existing jobs via HTTP GET

    curl http://istar.cse.cuhk.edu.hk/igrow/jobs


REST API for igrep
------------------

### Submit a new job via HTTP POST

    curl -d $'email=Jacky@cuhk.edu.hk&taxid=9606&queries=CTGCATGGTGGGGAAAAGGCATAGCCTGGG3
    AAAAGTGTTATGGGTTGTTTAATCAACCACTGAACTGCGGGGGTGACTAGTTATAACTTA6'
    http://istar.cse.cuhk.edu.hk/igrep/jobs

### Obtain existing jobs via HTTP GET

    curl http://istar.cse.cuhk.edu.hk/igrep/jobs


Licenses
--------

* Source code is licensed under [Apache License 2.0].
* Documentation is licensed under [CC BY 3.0].


Author
------

[Jacky Lee]


Logo
----

![istar logo](https://github.com/HongjianLi/istar/raw/master/logo.png)



[istar]: http://istar.cse.cuhk.edu.hk
[idock]: http://istar.cse.cuhk.edu.hk/idock
[igrep]: http://istar.cse.cuhk.edu.hk/igrep
[iview]: http://istar.cse.cuhk.edu.hk/iview
[Twitter Bootstrap]: https://github.com/twitter/bootstrap
[jQuery]: https://github.com/jquery/jquery
[jQuery UI]: https://github.com/jquery/jquery-ui
[three.js]: https://github.com/mrdoob/three.js
[zlib.js]: https://github.com/imaya/zlib.js
[jquery-dateFormat]: https://github.com/phstc/jquery-dateFormat
[jquery_lazyload]: https://github.com/tuupola/jquery_lazyload
[node.js]: https://github.com/joyent/node
[mongodb]: https://github.com/mongodb/node-mongodb-native
[express]: https://github.com/visionmedia/express
[validator]: https://github.com/chriso/node-validator
[spdy]: https://github.com/indutny/node-spdy
[MongoDB]: https://github.com/mongodb/mongo
[Apache License 2.0]: http://www.apache.org/licenses/LICENSE-2.0
[CC BY 3.0]: http://creativecommons.org/licenses/by/3.0
[Jacky Lee]: http://www.cse.cuhk.edu.hk/~hjli
