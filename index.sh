mkdir /home/`whoami`/Projects/page

touch /home/`whoami`/Projects/page/deviceslist.html
touch /home/`whoami`/Projects/page/plan.html

sudo ln -s /home/`whoami`/Projects/SmartSwitch/templates/master.html /var/www/html/index.html
sudo ln -s /home/`whoami`/Projects/page/deviceslist.html /var/www/html/deviceslist.html  
sudo ln -s /home/`whoami`/Projects/page/plan.html /var/www/html/plan.html 