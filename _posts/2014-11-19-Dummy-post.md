---
layout: post
title: "My first test post"
date: 2014-11-19 16:25:06 -0700
comments: true
---
Apply
=====

``` cpp
template <template <typename...> class MetaFunction, typename... Args>
using Apply = typename MetaFunction<Args...>::type;
```
