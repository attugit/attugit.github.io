.PHONY: build proof configure

build:
	bundle exec jekyll build

proof:
	bundle exec htmlproof ./_site

configure:
	bundle install
